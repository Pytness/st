- [x] st-font2-20190326-f64c2f8.diff

- [x] st-blinking_cursor-20230819-3a6d6d7.diff

- [x] st-kitty-graphics-20240922-a0274bc.diff
    - Included patches:
        - anysize:

            If you prefer the expected behavior, set anysize_halign and anysize_valign to 0 in config.h.

        - undercurl:

            This patch already supports underline colors and styles, so it is incompatible with the undercurl patch.

    - Patch Compatibility
        - Easy:
            - boxdraw
            - scrollback
            - alpha
            - background image

        - Medium:
            - ligatures

- [x] sixel

- [x] st-boxdraw_v2-0.8.5.diff

- [x] st-ligatures-boxdraw-20241226-0.9.2.diff

- [x] st-dynamic-cursor-color-0.9.diff

- [x] st-clickurl-0.8.5.diff

- [ ] st-universcroll-0.8.4.diff: PARTIALLY APPLIED

    When scrolling on an alt screen inside a terminal multiplexer like zellij, it will scroll the multiplexer instead of the altscreen.

    Otherwise it will function correctly.
