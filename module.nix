{
  lib,
  pkgs,
  config,
  ...
}:

let
  cfg = config.programs.yabar;

  mapExtra =
    v:
    lib.concatStringsSep "\n" (
      lib.mapAttrsToList (
        key: val:
        "${key} = ${if (builtins.isString val) then "\"${val}\"" else "${builtins.toString val}"};"
      ) v
    );

  listKeys = r: builtins.concatStringsSep "," (builtins.map (n: "\"${n}\"") (builtins.attrNames r));

  configFile =
    let
      bars = lib.mapAttrsToList (name: cfg: ''
        ${name}: {
          font: "${cfg.font}";
          position: "${cfg.position}";

          ${mapExtra cfg.extra}

          block-list: [${listKeys cfg.blocks}]

          ${builtins.concatStringsSep "\n" (
            lib.mapAttrsToList (name: cfg: ''
              ${name}: {
                exec: "${cfg.exec}";
                align: "${cfg.align}";
                ${mapExtra cfg.extra}
              };
            '') cfg.blocks
          )}
        };
      '') cfg.bars;
    in
    pkgs.writeText "yabar.conf" ''
      bar-list = [${listKeys cfg.bars}];
      ${builtins.concatStringsSep "\n" bars}
    '';
in
{
  options.programs.yabar = {
    enable = lib.mkEnableOption "yabar, a status bar for X window managers";

    package = lib.mkOption {
      default = pkgs.yabar;
      example = lib.literalExpression "pkgs.yabar";
      type = lib.types.package;

      description = ''
        The package which contains the `yabar` binary.
      '';
    };

    bars = lib.mkOption {
      default = { };
      type = lib.types.attrsOf (
        lib.types.submodule {
          options = {
            font = lib.mkOption {
              default = "sans bold 12";
              example = "Droid Sans, FontAwesome Bold 12";
              type = lib.types.str;

              description = ''
                The font that will be used to draw the status bar.
              '';
            };

            position = lib.mkOption {
              default = "top";
              example = "bottom";
              type = lib.types.enum [
                "top"
                "bottom"
              ];

              description = ''
                The position where the bar will be rendered.
              '';
            };

            extra = lib.mkOption {
              default = { };
              type = lib.types.attrsOf (lib.types.either lib.types.str lib.types.int);

              description = ''
                An attribute set which contains further attributes of a bar.
              '';
            };

            blocks = lib.mkOption {
              default = {};
              type = lib.types.attrsOf (
                lib.types.submodule {
                  options.exec = lib.mkOption {
                    example = "YABAR_DATE";
                    type = lib.types.str;
                    description = ''
                      The type of block to be executed.
                    '';
                  };

                  options.align = lib.mkOption {
                    default = "left";
                    example = "right";
                    type = lib.types.enum [
                      "left"
                      "center"
                      "right"
                    ];

                    description = ''
                      Whether to align the block at the left or right of the bar.
                    '';
                  };

                  options.extra = lib.mkOption {
                    default = { };
                    type = lib.types.attrsOf (lib.types.either lib.types.str lib.types.int);

                    description = ''
                      An attribute set which contains further attributes of a block.
                    '';
                  };
                }
              );

              description = ''
                Blocks that should be rendered by yabar.
              '';
            };
          };
        }
      );

      description = ''
        List of bars that should be rendered by yabar.
      '';
    };
  };

  config = lib.mkIf cfg.enable {
    systemd.user.services.yabar = {
      description = "yabar service";
      wantedBy = [ "graphical-session.target" ];
      partOf = [ "graphical-session.target" ];

      script = ''
        ${cfg.package}/bin/yabar -c ${configFile}
      '';

      serviceConfig.Restart = "always";
    };
  };
}
