# Camera

Current: third-person orbit behind the hero, right-drag adjusts yaw and
pitch, eye placed back and up from the target. Since Milestone 16 the 44
authored CameraArea volumes bias the look-at toward their control-point
centroid while the hero is inside one, easing in and out.

Original data: CameraArea volumes each own CamCtrlPoint nodes (193 in Level
1) through !^Owner^CameraArea. The authored extent is not stored, so the
runtime bounds each area by its control points plus padding. Cinematic
cameras live in .cff scripts and camera_lv1_* BDAEs, not yet decoded.
