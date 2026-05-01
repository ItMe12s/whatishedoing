# What is he doing?

Show other people what you are doing by automatically sending Discord webhook messages when you take common actions, such as playing a level or opening the editor. More options may be added later :3

## Setup guide

1. Create a Discord webhook (there's guides on YouTube)
2. Copy the webhook url
3. Hop on geode and get to this mod's settings page
4. Paste the url into the `Webhook URL` text box

## Profile manager

On this mod's settings page, open **Manage Profiles** to work with up to 10 named slots.
Don't forget to save a profile after you add/remove a level from the ID filter!

- **Save** stores a snapshot of your **last-applied** mod settings into that slot.
- **Load** applies a saved slot and closes open settings popups so values apply cleanly.
- **Rename** changes the slot's display name.
- **Delete** clears saved data for that slot.

## Currently logged actions

Each of these can be turned on or off in the mod settings under **Notification Toggles**.
There's also options to resize/disable sending screenshots.

- **Opening or exiting Geometry Dash** (with session time on close)
- **Starting or exiting a level** (level name, creator, ID when available, and time in the run)
- **Practice runs** (separate from normal)
- **Level complete** (different message for Normal, Practice, and Startpos runs)
- **New best** (below 100%)
- **Opening or exiting the editor** (play test and exit paths that leave the editor)

## Off by default / Community features

These are under **Other Notification Toggles** and below it.

- **Death** (dying after a configured percentage) (requested by Theitha)
- **Level ID filter** (All/Blacklist/Whitelist. Online levels use the check beside Favorite and Settings, Editor/local uses the check beside the level info button) (requested by many people)
- **Startpos progress** (minimum percentage from your startpos until death or completion, applies to death messages and **Startpos Complete**) (requested by Manscapers Discord server)
- **Extra URL slots** (In case you want to like let 5 servers/channels know what you're doing)

## Privacy / Security note

- DO NOT LET ANYONE SEE YOUR DISCORD WEBHOOK URL!!!
- All your data directly goes to Discord, not me
