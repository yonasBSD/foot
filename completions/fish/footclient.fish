complete -c footclient -x                            -a "(__fish_complete_subcommand)"
complete -c footclient -x -s t -l term               -a '(find /usr/share/terminfo -type f -printf "%f\n")' -d "value to set the environment variable TERM to (foot)"
complete -c footclient -x -s T -l title                                                                     -d "initial window title"
complete -c footclient -x -s a -l app-id                                                                    -d "value to set the app-id property on the Wayland window to (foot)"
complete -c footclient    -s m -l maximized                                                                 -d "start in maximized mode"
complete -c footclient    -s F -l fullscreen                                                                -d "start in fullscreen mode"
complete -c footclient    -s L -l login-shell                                                               -d "start shell as a login shell"
complete -c footclient -x -s w -l window-size-pixels                                                        -d "window WIDTHxHEIGHT, in pixels (700x500)"
complete -c footclient -x -s W -l window-size-chars                                                         -d "window WIDTHxHEIGHT, in characters (not set)"
complete -c footclient -F -s s -l server-socket                                                             -d "override the default path to the foot server socket ($XDG_RUNTIME_DIR/foot-$WAYLAND_DISPLAY.sock)"
complete -c footclient    -s H -l hold                                                                      -d "remain open after child process exits"
complete -c footclient -x -s d -l log-level          -a "info warning error"                                -d "log-level (info)"
complete -c footclient -x -s l -l log-colorize       -a "always never auto"                                 -d "enable or disable colorization of log output on stderr"
complete -c footclient    -s v -l version                                                                   -d "show the version number and quit"
complete -c footclient    -s h -l help                                                                      -d "show help message and quit"
