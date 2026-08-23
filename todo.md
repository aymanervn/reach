for each item in the tasks list you will do the following steps:

A. read and understand the intent of the item
B. reach the code relevant and plan around it and understand the ecosystem around it
C. decide where it should live according to reach standards what it can do and cannot do
D. implement it
E. clean up any old implementations or stale code or dead lines and migrate if anything needs migrating
F. compile test
G. dont add tests unless they are for logic that can break, dont add any UI tests
H. after clean up and verification of the clean up, and compile test, commit using the standard commit names we use, like "feat:"
s
Here are the tasks that you will do in one goal, the criteria of success of the goal is to complete all of them, aside from the ones marked skippable if needed:

1. there is a bug in the quick settings popup, after opening it, when you click on a button that expands the quick settings height like app volumes, the whole menu expands which is expected, but it also moves down a bit. visual bug and it hints at maybe bad code somewhere in that path.
2. there is an error popup that shows when you open reach without installing i think, and it says reach must be started with -install, this should be corrected.
3. we have a shadows implementation, and I believe it was applied to dock topbar, launcher and clipboard, if its not added to them then add it and clean up
4. I noticed we have the title of pinned windows in reach.ini config, is that needed? adjust after weighing the reason
5. in the settings app, we have energy page, but it doesnt provide the option to configure the screen off timer. add it, and change the defaults to be my current values, and make the default on the screen timer to be 10min
6. add a text measure helper, somewhere, because currently in topbar especially we are inferring many times the size of the text to make the paddings uniform but they end up not uniform, if we have the text width measure function we can adjust this perfectly, add it and do so, adjust top bar and also the dock on icon hover
7. make sure the launcher results list items support right clicking, I noticed sometimes it works sometimes it doesnt, not a major problem if its not obvious skip
8. currently tray items are a feature which then gets opened and positioned inside the top bar. we need to make the tray icons a service, and the topbar will use the service that extracts icons, using icon manager that caches and all. so there will be no tray icons feature. its part of the topbar exactly the way it looks nice, only difference its part of the topbar now and the service provides all the data we need. this takes time to plan, so dont rush it go slowly and trust the implementation as it was created after careful probing.
9. currently in the top bar, for laptops we show battery, we show it in yellow when its in battery saver and red when low below 20 but we dont show when its charging, lets just make it blue. also, the design will be changed, current one is good, but lets make the battery pill the same silouhette as the language pill, just wider and dont add a notch on the right, simple pill that is same background as the language pill and fills with the colors we talked about, and on top in the middle of it, the percentage number, for both dark and light themes. always make sure we are theme aware dont forget that.
10. in the topbar system stats, we should instead of cpu percentage usage, we make it cpu temperature, but for this you might need to probe and see if we have a stable api, but if we need to handle each manufacturer, and it takes many steps that are fragile, or performance intensive or cause micro stutters then skip this entirely and keep the usage percentage.
11. when reach opens first time it reads default wallpaper and uses it, but if we have multiple monitors windows stores the wallpaper in a weird way, maybe, because i noticed the end result we get, for example if we have two monitors, i see both wallpapers glued together as one wallpaper on the main monitor
