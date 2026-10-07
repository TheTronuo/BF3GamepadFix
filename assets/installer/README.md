# Installer icon assets

Installer assets prepared with the built-in ImageGen editor.

- xbox360.png: white flat controller reference from
  https://www.clipartmax.com/png/middle/344-3448053_vector-freeuse-download-xbox-clipart-for-free-download-xbox-360-controller-png.png
  (the original Vectorified image host did not respond; this is the matching flat white controller style).
- ps3.png: flat DualShock 3 reference from KP Studio:
  https://www.behance.net/gallery/40210503/Illustration-PlayStation-Controller
  https://mir-s3-cdn-cf.behance.net/project_modules/max_632_webp/231f8640210503.577607890f3aa.png

Edit prompts: isolate the selected controller, remove checkerboard/white background,
background emblems, bottom caption and watermarks; preserve the flat controller,
controls and colored buttons; produce a tightly framed transparent PNG with no
extra objects or shadow. Separate built-in ImageGen edit calls were used for
Xbox 360 and PS3.

These are processed raster versions, not pixel-identical crops of the references.
The UI trims transparent padding when rendering and embeds both PNGs inside the
single EXE. Original reference images are kept locally and are not versioned.
No redistribution license is asserted by these notes.
