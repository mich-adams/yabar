{
    inputs.nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";

    outputs =
    {
	self,
	nixpkgs,
    }:
    let
	overlays = [
	self.overlays.default
	];
    pkgsFor = system: import nixpkgs { inherit system overlays; };

    targetSystems = [
	"aarch64-linux"
	    "x86_64-linux"
    ];
    in
    {
	overlays.default = final: prev: { inherit (self.packages.${prev.system}) yabar; };

	packages = nixpkgs.lib.genAttrs targetSystems (
		system:
		let
		pkgs = pkgsFor system;
		version = "0.4.0";
		in
		rec {
		yabar = pkgs.stdenv.mkDerivation {
		version = version;
		pname = "yabar";
		src = ./.;
		strictDeps = true;
		depsBuildBuild = with pkgs; [
		pkg-config
		];
		nativeBuildInputs = with pkgs; [
		pkg-config
		asciidoc
		docbook_xsl
		libxslt
		makeWrapper
		libconfig
		pango
		];
		buildInputs = with pkgs; [
		    cairo
			gdk-pixbuf
			libconfig
			pango
			xorg.xcbutilwm
			libxkbcommon
			alsa-lib
			wirelesstools
			playerctl
		];

		hardeningDisable = [ "format" ];
		makeFlags = ["PLAYERCTL=1" "DESTDIR=$(out)" "PREFIX=/"];
		postPatch = ''
		    substituteInPlace ./Makefile \
		    --replace "\$(shell git describe)" "${version}" \
		    --replace "a2x" "a2x --no-xmllint"
		    '';

		};
		default = yabar;
		}
	);

	nixosModules.default = ./module.nix;

	devShells = nixpkgs.lib.genAttrs targetSystems (
		system:
		let
		pkgs = pkgsFor system;
		in
		{
		default = pkgs.mkShell {
		inputsFrom = [ self.packages.${system}.yabar ];
		packages = with pkgs; [
		deno
		mdbook
		zbus-xmlgen
		];

		};
		}
		);

    };
}
