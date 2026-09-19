# Tools

| tool | purpose |
|---|---|
| extract_packs.sh | unpack .pack archives (GBMP to ZIP swap) into Assets/ |
| extract_videos.sh | copy the boot movies out of the IPA |
| verify_assets.sh | check an Assets tree before building |
| check_staged.sh | local compile check matching CI |
| dump_bdae.py | inspect a BDAE: tables, images, clips, nodes |
| dump_scene.py | list a level's authored nodes by GameType |
| dump_vox.py | list sound events, verify clips exist |
| inspect_atlas.py | decode UI atlases to gridded sheets and labelled crops |
| disasm_libspiderman.py | disassemble the Android engine with resolved strings |
| parse_bdae324.py, parse_irr.py | early format explorers kept for reference |
| inspect_config_bins.py, catalog_assets.py | early survey scripts kept for reference |

Python tools need `pip install pillow capstone pyelftools` where noted.
