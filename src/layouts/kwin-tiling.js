// Mainuan Welcome "Tiling" layout, loaded through org.kde.kwin.Scripting.
// Sets KWin's native tiles to one large tile and two stacked tiles on every
// screen and virtual desktop. Windows snap into them when dragged with Shift
// held (Meta+T edits the tiles). KWin 6 has no automatic tiling of its own.
const HORIZONTAL = 1;
const VERTICAL = 2;
for (const output of workspace.screens) {
    for (const desktop of workspace.desktops) {
        const root = workspace.rootTile(output, desktop);
        if (!root) {
            continue;
        }
        while (root.tiles.length > 0) {
            root.tiles[0].remove();
        }
        root.split(HORIZONTAL);
        if (root.tiles.length === 2) {
            root.tiles[1].split(VERTICAL);
        }
    }
}
print("mainuan-welcome: tiling layout ready");
