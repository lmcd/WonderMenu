# WonderMenu - Flashcart menu for the N64 #

WonderMenu is in BETA and mileage may vary!

## Features ##
 * Beautiful/responsive interface running at 480i, 60fps
 * Extra visual enhancements on Analogue 3D with 32bit colour and progressive output
 * Comprehensive retail games database with high resolution artwork and metadata
 * Cheats database for hundreds of games
 * Ability to capture screenshots mid-game
 * Ability to playback tool assisted speedrun files from TASVideos.org
 * Works on SummerCart64 - other carts coming soon

## Building ##

WonderMenu depends on custom versions of `tiny3d` and `libdragon`.

https://github.com/lmcd/libdragon/tree/fat_updates

https://github.com/lmcd/tiny3d/tree/viewport_attach_no_scissor

With these two libraries installed into `N64_INST`, you should be able to `make` WonderMenu.
