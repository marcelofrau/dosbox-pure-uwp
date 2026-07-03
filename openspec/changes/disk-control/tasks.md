# Tasks — Disk Control

- [ ] 1. Handle `RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE` in retro_env switch
- [ ] 2. Implement disc storage (vector of RetroDisc structs with path/label/data)
- [ ] 3. Implement set_eject_state / get_eject_state
- [ ] 4. Implement get_image_index / set_image_index (with index clamping + content load)
- [ ] 5. Implement get_num_images
- [ ] 6. Implement replace_image_index / add_image_index
- [ ] 7. Implement set_initial_image (boot disc + path storage)
- [ ] 8. Implement get_image_path / get_image_label
- [ ] 9. Handle disc change notification to core (pending_disc_change flag consumed on next retro_run)
- [ ] 10. Build and test with multi-disc game (.cue + .bin or .m3u)
