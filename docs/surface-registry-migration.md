# Surface registry migration

The registry assigns runtime slots from the order of its definitions. The registry and interfeature routes use typed keys to identify surfaces. When the Dock or Top Bar opens the Context Menu, the route supplies the owning control for layout and dismissal.

## Lifecycle

- Normal shutdown stops features, configuration, search, icons, app control and Idle Watch before stopping Now Playing, system controls, audio, foreground watching and the window manager. Surface windows hide before the wallpaper.
- Start and stop are guarded against duplicate calls. A failed start unwinds the runtime steps it completed, unbinds callbacks and leaves workers created with the host available for a retry. Startup apps are scheduled after the input source starts successfully.
- The dependency factory creates native windows in registry order. Positive layout layers are restacked explicitly. The Top Bar starts in the Windows app band at layer zero.
- Surface teardown releases the renderer and its compositor target before destroying the target window. Failed app creation releases the dependencies it already acquired.

## Event and layout contracts

- Service notifications for configuration, foreground, stats and Now Playing are host-wide. The first surface window that accepts the post carries the wakeup. Dispatch is independent of the source surface.
- Launcher Enter, arrows and text events come from its own window. Quick Settings handles Enter and text events from its own window. Escape uses the shell-wide event routes.
- The pre-migration game-mode event filter permits app-switch gestures and drops disallowed events before surface dispatch.
- The layout manager can hold 32 participants, including surfaces and edge reveals. Registration failure aborts host creation so the host never starts with an incomplete layout.

Settings window branding lives in the concrete Windows factory called by the Settings app. The system action executor consumes the shared commands from `core/menu_commands.h`.
