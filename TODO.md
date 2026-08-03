# TODO

## General

- [x] Views all currently have absolute positions. Should instead respect relative positions and draw using `finalFrame`
- [x] When this is done, remove `yRenderOffset` and fix broken scissor during `GameInfoTransitionScene`.
- [ ] Off-screen rendered views. Views should be able to opt-in to off-screen rendering. The final view will be composited as a texture. This is useful when fake-making views with rounded corners that need variable-opacity - like ScreenshotThumbnailView
- [ ] When this is done, finish `ScreenshotsImportPopover`
- [ ] Layout passes. Each full render should do a layout pass to determine what has moved and call `layoutSubviews` where needed
- [ ] Add 64DD cartridge model
- [ ] Add Netherlands flag
- [ ] Come up with a way of determining if overclocking is enabled for A3D, and infer 32-bit colour from that
- [ ] Investigate buttonDown and buttonUp methods on `Scene` in place of `InputWatcher`
- [ ] Use references everywhere where appropriate instead of pointers (I was a C++ noob entering this project)
- [ ] Account for overscan in Scene::view

## Views
- [x] `ScrollbarView` should be simplified with a `RectView`
- [ ] Two-stage combiner for `NumberView`
- [x] `TabControlView` should be refactored to use `LabelView`s
- [x] Combine drawing logic of `LabelView` and `LabelReferenceView`
- [ ] `ListScene` should use a `TableView` like everything else
- [ ] Add `VStackView` and `HStackView`
- [ ] Add `ScrollView` that manages scrollbars automatically

## Models

- [ ] Add 64DD cartridge model
- [ ] Add missing geometry for ridges on main cart (my Fast64 setup is currently borked :/)
- [ ] Add back label on main cart
- [ ] Experiment with fake bump-mapping for cartridge lines. Definitely possible

## Labels/Artwork

- [ ] Make cooler looking question mark label
- [ ] Detect libdragon games and add badge
