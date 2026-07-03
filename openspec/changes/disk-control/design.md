# Design — Disk Control

## retro_disk_control_ext_callback
```cpp
struct RetroDisc {
    std::wstring path;
    std::string label;
    std::vector<uint8_t> data; // loaded content
};

struct DiskControlState {
    retro_disk_control_ext_callback callbacks;
    std::vector<RetroDisc> discs;
    unsigned current_index = 0;
    bool ejected = false;
    bool initialized = false;
};
```

## SET_DISK_CONTROL_EXT_INTERFACE (cmd 81)
In `retro_env` switch: `case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE:` — cast `data` to `retro_disk_control_ext_callback*`, store all 10 function pointers.

## Callback Implementations

| Callback | Implementation |
|----------|---------------|
| set_eject_state(bool e) | Set `ejected = e`. If ejecting, invalidate current content. |
| get_eject_state() | Return `ejected`. |
| get_image_index() | Return `current_index`. |
| set_image_index(unsigned i) | Clamp to [0, discs.size()). Set `current_index = i`. Load disc data, notify core disc changed. |
| get_num_images() | Return `discs.size()`. |
| replace_image_index(unsigned i, retro_game_info* info) | Replace disc at index i. Load new content from `info->data` or `info->path`. |
| add_image_index() | `discs.emplace_back()`. Return new index. |
| set_initial_image(unsigned i, const char* path) | Set boot disc. Store path, load content, set `current_index = i`. |
| get_image_path(unsigned i) | Return `discs[i].path.c_str()`. |
| get_image_label(unsigned i) | Return `discs[i].label.c_str()`. |

## Content Loading
Disc content loaded when `set_image_index` / `replace_image_index` / `set_initial_image` called. If `info->data` provided, use it. Otherwise load from `info->path` via UWP VFS.

## Disc Change Notification
After switching disc, call `retro_run` flow's internal disc change handler or set `pending_disc_change` flag consumed on next retro_run.
