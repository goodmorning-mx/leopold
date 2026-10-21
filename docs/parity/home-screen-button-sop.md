# Home Screen Button SOP

Use this standard for every circular Home control, including Clock, Navigation, Weather, USB, WhatsApp, Spotify, and Settings.

This is the project-level visual and behavior contract for the `$leopold-home-controls` skill. Shared geometry and rendering values belong in `shared/rendering.json` and the shared screen/asset manifests; generated frontend/device outputs are consumers, not independent sources.

## Appearance

- Keep the existing control centers, 72 × 72 touch bounds, and assigned icon artwork.
- Use a translucent cool-glass disc with a soft light center, a thin high-contrast rim, and one restrained inner highlight.
- Keep the selected wallpaper visible through the glass. Do not place an opaque backing behind a control.
- Do not add a cast shadow or colored glow in the resting state. The glass edge and icon must define the control; avoid a dark shadow that appears detached from the disc on the wallpaper.
- Keep the icon centered, at least 40 px visible within the 72 px hit area, and bright enough to read over light and dark parts of every supported wallpaper.
- Keep brand marks in their original colors; use a neutral light icon for utility controls.
- Keep icons reactive to their existing shared state. When a control has active/inactive or connected/disconnected states, reflect that state through its assigned artwork treatment without changing the icon identity or inventing an unrelated color.

## Interaction

- Show a clear pressed state while a finger or pointer is down: brighten the rim and glass surface without moving the control or changing its icon.
- On release inside the same hit area, perform that control's existing action. A release outside cancels the action.
- On release or cancellation, restore the resting treatment. Do not leave controls visually selected after the pointer is no longer down unless the product's shared state says that control is active.
- Home status indicators open their matching connectivity panel when tapped; they must not be decorative dead zones.
- Keep Home art, icons, pressed feedback, and hit bounds aligned between the simulator and device renderer.

## Review

- Check all seven controls on Flower, Chevron, and Mechanical Flower backgrounds in both appearance modes.
- Check the resting, pressed, cancellation, and applicable active/inactive states; icon visibility, touch alignment, and each destination.
- Capture the simulator and device with the same screen, background, and state before approving visual parity.
