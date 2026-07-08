package com.armsx2.ui

import android.content.Context
import android.net.Uri
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.gestures.animateScrollBy
import androidx.compose.foundation.gestures.scrollBy
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.lazy.itemsIndexed as lazyItemsIndexed
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.blur
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.scale
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.vector.PathParser
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.documentfile.provider.DocumentFile
import coil.compose.AsyncImage
import coil.compose.SubcomposeAsyncImage
import coil.request.ImageRequest
import com.armsx2.R
import com.armsx2.EmuState
import com.armsx2.FilenameParser
import com.armsx2.CoverArtStyle
import com.armsx2.CustomCovers
import com.armsx2.GameInfo
import com.armsx2.LibraryRecentShelf
import com.armsx2.LibraryTitles
import com.armsx2.LibraryView
import com.armsx2.GamePlatform
import com.armsx2.Main
import com.armsx2.i18n.I18n
import com.armsx2.i18n.str
import kr.co.iefriends.pcsx2.NativeApp
import org.json.JSONArray
import org.json.JSONObject
import android.os.Build
import android.os.Environment
import android.os.ParcelFileDescriptor
import com.armsx2.BuildConfig
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.sin

private val GAME_EXTENSIONS = setOf(
    "iso", "chd", "cso", "zso", "gz", "bin", "mdf", "img", "nrg", "dump",
    // ELF/homebrew: scanned serial-less (the disc probe skips .elf), so they show
    // in the main library with long-press per-game settings + custom covers. A
    // scanned content:// ELF is copied to a real .elf file at launch (#253).
    "elf",
)

/** Max subdirectory depth the ROM scan descends. Deep enough for any sane
 *  "one game per folder" / "by-letter" organisation, capped so a pathological
 *  tree (or a SAF mount that loops) can't make the scan run away. */
private const val MAX_SCAN_DEPTH = 12

// Display font for the under-cover game titles: Bebas Neue (SIL Open Font
// License). All-caps + condensed, so long titles fit; OFL is fork-safe and
// redistributable, unlike the earlier commercial font.
private val TitleFont = FontFamily(Font(R.font.bebas_neue))

object GamesList {
    private const val KEY_LIBRARY_BACKGROUND = "library.background.path"
    /**
     * Cached list of games for the configured ROMs folder. Backed by
     * SharedPreferences (`gamesCache` JSON + `gamesCacheDir` URI) so a
     * cold start doesn't re-walk every file. The list is only refreshed
     * when:
     *   - the user changes their ROMs folder (auto-detected via the
     *     romsDir state — cached dir mismatch triggers a rescan), or
     *   - they explicitly tap the Refresh card in the grid.
     */
    private val games = mutableStateListOf<GameInfo>()
    private val scanning = mutableStateOf(false)
    private val scanError = mutableStateOf<String?>(null)
    private val lastScannedRoms = mutableStateOf<String?>(null)
    private val cacheLoaded = mutableStateOf(false)
    private val recentUris = mutableStateListOf<String>()
    private val recentLoaded = mutableStateOf(false)
    private val customBackgroundPath = mutableStateOf<String?>(null)
    private val customBackgroundLoaded = mutableStateOf(false)
    // Preloaded custom-cover index (lowercased stem -> file), refreshed on
    // CustomCovers.version. Cover tiles look this up synchronously instead of each
    // doing its own dir listing mid-scroll (which raced and mis-assigned covers).
    private val customCoverMap = mutableStateOf<Map<String, File>>(emptyMap())
    private val controllerSelectedUri = mutableStateOf<String?>(null)
    private data class ControllerGameRow(
        val rowId: Int,
        val games: List<GameInfo>,
        val listItemIndex: Int,
    )
    private data class ControllerGamePosition(
        val rowIndex: Int,
        val columnIndex: Int,
        val game: GameInfo,
    )
    private var controllerRows: List<ControllerGameRow> = emptyList()
    private var controllerRowIndex = 0
    private var controllerColumnIndex = 0
    private val controllerScrollVelocity = mutableStateOf(0f)

    // Controller focus zones: the top toolbar chips, the game grid, and the
    // bottom-left gear (global settings). The grid is the manual game model;
    // TOOLBAR/RAIL are the non-game buttons wired in here so the controller can
    // reach them. UP from the top grid row enters TOOLBAR; DOWN from the last
    // row enters RAIL.
    private enum class Zone { TOOLBAR, GRID, RAIL }
    private val controllerZone = mutableStateOf(Zone.GRID)
    private val controllerToolbarIndex = mutableStateOf(0)
    // The left rail holds an info button (top), an "Aa" titles toggle (middle),
    // and the gear (bottom). On small screens the old help text was tall enough
    // to push the gear off-screen, so it now lives behind the info button as its
    // own screen. 0 = info, 1 = titles, 2 = list view, 3 = gear/settings. (The
    // Recently-Played toggle lives in the TOP toolbar now, not the rail.)
    private val railSelection = mutableStateOf(3)
    private val infoDialogOpen = mutableStateOf(false)

    // ---- #267 Library search ----------------------------------------------
    // Query filters the library grid live (title OR serial, case-insensitive).
    // The panel is a gamepad-first on-screen keyboard (RetroArch-style): D-pad
    // moves the key focus, A presses, B closes-and-clears, "Done" closes but
    // KEEPS the filter; every key is also tappable for touch users. Opened from
    // the toolbar Search chip or the Y/Triangle button in the library.
    val searchOpen = mutableStateOf(false)
    val searchQuery = mutableStateOf("")
    private val searchKbRow = mutableStateOf(0)
    private val searchKbCol = mutableStateOf(0)
    private val searchKeys: List<List<String>> = listOf(
        listOf("A", "B", "C", "D", "E", "F", "G", "H", "I", "J"),
        listOf("K", "L", "M", "N", "O", "P", "Q", "R", "S", "T"),
        listOf("U", "V", "W", "X", "Y", "Z", "0", "1", "2", "3"),
        listOf("4", "5", "6", "7", "8", "9", "Space", "Del", "Clear", "Done"),
    )

    fun openSearch() {
        searchOpen.value = true
        searchKbRow.value = 0
        searchKbCol.value = 0
    }

    private fun searchKeyPress(key: String) {
        when (key) {
            "Space" -> searchQuery.value += " "
            "Del" -> searchQuery.value = searchQuery.value.dropLast(1)
            "Clear" -> searchQuery.value = ""
            "Done" -> searchOpen.value = false // keep the filter active
            else -> searchQuery.value += key
        }
    }
    // Toolbar actions, published from HeaderRow composition so they capture the
    // live ActivityResult launchers / context. Label + click action, in order.
    private var controllerToolbarActions: List<Pair<String, () -> Unit>> = emptyList()

    fun controllerActive(): Boolean =
        WindowImpl.showLibrary.value || Main.eState.value == EmuState.STOPPED

    /** Reset focus to the game grid (call when the library (re)appears). */
    fun resetControllerZone() {
        controllerZone.value = Zone.GRID
        controllerToolbarIndex.value = 0
    }

    fun handleControllerMove(dx: Int, dy: Int): Boolean {
        if (!controllerActive()) return false

        // Search keyboard captures nav while open (D-pad moves the key focus).
        if (searchOpen.value) {
            if (dy != 0) searchKbRow.value = (searchKbRow.value + dy).coerceIn(0, searchKeys.lastIndex)
            if (dx != 0) {
                val rowLast = searchKeys[searchKbRow.value].lastIndex
                searchKbCol.value = (searchKbCol.value.coerceIn(0, rowLast) + dx).coerceIn(0, rowLast)
            }
            return true
        }

        when (controllerZone.value) {
            Zone.TOOLBAR -> {
                if (dy > 0) {
                    controllerZone.value = Zone.GRID
                } else if (dx != 0) {
                    val last = (controllerToolbarActions.size - 1).coerceAtLeast(0)
                    controllerToolbarIndex.value =
                        (controllerToolbarIndex.value + dx).coerceIn(0, last)
                }
                return true
            }
            Zone.RAIL -> {
                // Info (top) + titles toggle + list/shelf toggle + gear (bottom)
                // live on the rail. Up/down move between them; right returns to grid.
                if (dy != 0) railSelection.value = (railSelection.value + if (dy < 0) -1 else 1).coerceIn(0, 3)
                if (dx > 0) controllerZone.value = Zone.GRID
                return true
            }
            Zone.GRID -> { /* handled below */ }
        }

        val rows = controllerRows.filter { it.games.isNotEmpty() }
        if (rows.isEmpty()) {
            // No games yet — up reaches the toolbar, left reaches the gear, so
            // the controller can scan / open setup on a fresh install.
            if (dy < 0) controllerZone.value = Zone.TOOLBAR
            else if (dx < 0) { railSelection.value = 3; controllerZone.value = Zone.RAIL }
            return true
        }

        val current = controllerSelectedPosition(rows)
        if (dy != 0) {
            if (dy < 0 && current.rowIndex == 0) {
                controllerZone.value = Zone.TOOLBAR
                return true
            }
            val rowIndex = (current.rowIndex + dy).coerceIn(0, rows.lastIndex)
            val columnIndex = current.columnIndex.coerceIn(0, rows[rowIndex].games.lastIndex)
            selectControllerPosition(rows, rowIndex, columnIndex)
        } else if (dx != 0) {
            if (dx < 0 && current.columnIndex == 0) {
                // Left off the first cover of any shelf → the gear on the rail.
                railSelection.value = 3
                controllerZone.value = Zone.RAIL
                return true
            }
            val row = rows[current.rowIndex]
            val columnIndex = (current.columnIndex + dx).coerceIn(0, row.games.lastIndex)
            selectControllerPosition(rows, current.rowIndex, columnIndex)
        }
        return true
    }

    fun handleControllerConfirm(): Boolean {
        if (!controllerActive()) return false
        if (handleSearchConfirm()) return true
        when (controllerZone.value) {
            Zone.TOOLBAR ->
                controllerToolbarActions.getOrNull(controllerToolbarIndex.value)?.second?.invoke()
            Zone.RAIL ->
                when (railSelection.value) {
                    0 -> infoDialogOpen.value = true
                    1 -> LibraryTitles.set(!LibraryTitles.show.value)
                    2 -> LibraryView.toggleListMode()
                    else -> InGameOverlay.openGlobalSettings()
                }
            Zone.GRID -> {
                val rows = controllerRows.filter { it.games.isNotEmpty() }
                if (rows.isNotEmpty()) launchGame(controllerSelectedPosition(rows).game)
            }
        }
        return true
    }

    /** A/confirm while the search keyboard is open: press the focused key.
     *  Checked BEFORE the zone dispatch by handleControllerConfirm. */
    private fun handleSearchConfirm(): Boolean {
        if (!searchOpen.value) return false
        val row = searchKeys[searchKbRow.value]
        searchKeyPress(row[searchKbCol.value.coerceIn(0, row.lastIndex)])
        return true
    }

    /** Open per-game settings for the currently-highlighted cover (controller
     *  equivalent of long-pressing a game). Used by the Menu hotkey / Y button
     *  while browsing the library. */
    fun openSelectedGameSettings(): Boolean {
        if (searchOpen.value) return true // swallow X while the search panel is up
        if (!controllerActive() || controllerZone.value != Zone.GRID) return false
        val rows = controllerRows.filter { it.games.isNotEmpty() }
        if (rows.isEmpty()) return false
        InGameOverlay.openGameSettings(controllerSelectedPosition(rows).game)
        return true
    }

    fun handleControllerBack(): Boolean {
        if (!controllerActive()) return false
        // Search panel: B closes it AND clears the filter ("Done" keeps it).
        if (searchOpen.value) {
            searchOpen.value = false
            searchQuery.value = ""
            return true
        }
        // The info screen captures back/B first so it can be dismissed.
        if (infoDialogOpen.value) {
            infoDialogOpen.value = false
            return true
        }
        // From the toolbar/gear, back returns to the game grid first.
        if (controllerZone.value != Zone.GRID) {
            controllerZone.value = Zone.GRID
            return true
        }
        if (WindowImpl.showLibrary.value)
            WindowImpl.showLibrary.value = false
        return true
    }

    fun handleControllerScroll(velocity: Float): Boolean {
        if (!controllerActive()) {
            controllerScrollVelocity.value = 0f
            return false
        }
        controllerScrollVelocity.value = if (abs(velocity) > 0.08f) velocity.coerceIn(-1f, 1f) else 0f
        return true
    }

    @Composable
    fun GamesRow() {
        val context = LocalContext.current
        val romsDirs = Main.romsDirs.value

        LaunchedEffect(Unit) {
            if (!customBackgroundLoaded.value) {
                customBackgroundLoaded.value = true
                loadCustomBackground(context)
            }
        }

        // Preload the custom-cover index off the main thread; refresh when a cover
        // is added/removed (CustomCovers.version bump). Cover tiles read this map
        // synchronously, so they don't each list the dir mid-scroll (which raced
        // and mis-assigned covers across games).
        LaunchedEffect(CustomCovers.version.value) {
            customCoverMap.value = withContext(Dispatchers.IO) { CustomCovers.loadAll(context) }
        }

        // Stable cache key — order-independent join of all configured dirs.
        // Two-dir configs in either order hit the same cache, single-dir
        // matches the old format. Used for both "is the cache stale?"
        // checks and the cache write below.
        val romsKey = remember(romsDirs) { cacheKeyForDirs(romsDirs) }

        LaunchedEffect(romsKey) {
            if (romsDirs.isEmpty()) return@LaunchedEffect

            if (!recentLoaded.value) {
                recentLoaded.value = true
                loadRecents()
            }

            if (!cacheLoaded.value) {
                cacheLoaded.value = true
                val (cachedKey, cachedGames) = loadCache(context)
                if (cachedKey == romsKey) {
                    // Show the cached list instantly for a snappy open, then fall
                    // through to a background rescan so games added to the folder
                    // since last launch are picked up automatically (issue #223) —
                    // no manual Refresh tap needed, and no blocking wait.
                    games.clear()
                    games.addAll(cachedGames)
                }
            }

            if (lastScannedRoms.value != romsKey && !scanning.value) {
                scanRoms(context, romsDirs, romsKey)
            }
        }

        Box(
            Modifier
                .fillMaxSize()
                .let { if (WindowImpl.overlayVisible.value) it.blur(18.dp) else it }
                .background(
                    Brush.verticalGradient(
                        listOf(
                            Color(0xFF001124),
                            Color(0xFF002647),
                            Color(0xFF000711),
                        ),
                    ),
                )
                .padding(16.dp),
        ) {
            when {
                romsDirs.isEmpty() -> EmptyMessage(
                    str("games.empty.noFolders.title"),
                    str("games.empty.noFolders.body"),
                )
                scanning.value && games.isEmpty() -> ScanningSpinner()
                scanError.value != null -> Text(
                    scanError.value!!,
                    color = Color(0xFFFF6B6B),
                )
                else -> {
                    LibraryScreen(context, romsDirs, romsKey)
                }
            }
        }
    }

    /** Sorted "|" join so ["A","B"] and ["B","A"] produce the same key.
     *  We dedupe within the user's selection in case they accidentally
     *  pick the same folder twice. The "|v3" suffix invalidates legacy
     *  caches that were built before .img/.mdf/.nrg/.dump were probed for
     *  serials and before DMC2 Dante/Lucia filename fallback landed — bump
     *  again any time the probe coverage changes. */
    /** github (all-files) build with All-Files Access actually granted: read ROMs
     *  via raw /storage paths instead of SAF, because holding the permission evicts
     *  the persisted SAF tree grant so openFileDescriptor() throws. Play (flavor
     *  STORAGE_ALL_FILES=false) and github-without-the-grant keep the SAF path. */
    private fun allFilesRomMode(): Boolean =
        BuildConfig.STORAGE_ALL_FILES &&
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.R &&
            Environment.isExternalStorageManager()

    // The "|raw" suffix keeps the raw-mode cache (file:// URIs) separate from the
    // SAF cache (content:// URIs), so toggling All-Files Access can't make the
    // launch path use a URI from the wrong mode.
    private fun cacheKeyForDirs(dirs: List<String>): String =
        dirs.toSet().sorted().joinToString("|") + "|v4" + if (allFilesRomMode()) "|raw" else ""

    @Composable
    private fun LibraryScreen(context: Context, romsDirs: List<String>, romsKey: String) {
        BoxWithConstraints(Modifier.fillMaxSize()) {
            val landscape = maxWidth >= maxHeight
            if (landscape) {
                Row(
                    Modifier
                        .fillMaxSize()
                        .clip(RoundedCornerShape(28.dp))
                        .background(Color(0xFF00101F)),
                ) {
                    LibraryNavRail(landscape = true)
                    LibraryContent(
                        context = context,
                        romsDirs = romsDirs,
                        romsKey = romsKey,
                        landscape = true,
                        modifier = Modifier.weight(1f),
                    )
                }
            } else {
                Box(
                    Modifier
                        .fillMaxSize()
                        .clip(RoundedCornerShape(28.dp))
                        .background(Color(0xFF00101F)),
                ) {
                    LibraryContent(
                        context = context,
                        romsDirs = romsDirs,
                        romsKey = romsKey,
                        landscape = false,
                        modifier = Modifier
                            .fillMaxSize()
                            .padding(bottom = 86.dp),
                    )
                    LibraryNavRail(
                        landscape = false,
                        modifier = Modifier.align(Alignment.BottomCenter),
                    )
                }
            }
            if (infoDialogOpen.value) {
                InfoScreen(onDismiss = { infoDialogOpen.value = false })
            }
        }
    }

    @Composable
    private fun LibraryContent(
        context: Context,
        romsDirs: List<String>,
        romsKey: String,
        landscape: Boolean,
        modifier: Modifier = Modifier,
    ) {
        // Fit-per-row: size each shelf to the covers that actually fit the screen
        // width, so games wrap onto MORE shelves and the user only scrolls DOWN.
        // A manual column count (LibraryView.columns) overrides the auto fit and
        // drives cover size; list view collapses to one game per row (names only).
        val screenWidthDp = LocalConfiguration.current.screenWidthDp
        val screenHeightDp = LocalConfiguration.current.screenHeightDp
        val listMode = LibraryView.listMode.value
        val userCols = LibraryView.columns.value
        val userRows = LibraryView.rows.value
        val coverPlusGapDp = (if (landscape) 92 else 98) + 28
        val navRailDp = if (landscape) 88 else 0
        val availDp = (screenWidthDp - navRailDp - 52).coerceAtLeast(coverPlusGapDp)
        val autoPerRow = ((availDp + 28) / coverPlusGapDp).coerceAtLeast(1)
        val perRow = when {
            listMode -> 1
            userCols > 0 -> userCols
            else -> autoPerRow
        }
        // Manual grid → derive cover width to fit `perRow` across, optionally capped
        // so `userRows` rows fit the screen height (CoverArt aspect = 0.7 = w/h).
        val customCoverW: Dp? = if (!listMode && (userCols > 0 || userRows > 0)) {
            val byCol = (availDp - 28 * (perRow - 1)).toFloat() / perRow
            val byRow = if (userRows > 0) {
                val chrome = if (landscape) 150 else 180
                val availH = (screenHeightDp - chrome).coerceAtLeast(160).toFloat()
                (availH / userRows) * 0.7f
            } else Float.MAX_VALUE
            minOf(byCol, byRow).coerceIn(48f, 220f).dp
        } else null
        // #267: live search filter — title OR serial, case-insensitive. Applies to
        // the library grid (and its controller model) as you type.
        val searchQ = searchQuery.value.trim()
        val visibleGames = if (searchQ.isEmpty()) games.toList() else games.filter {
            it.title.contains(searchQ, ignoreCase = true) ||
                (it.serial?.contains(searchQ, ignoreCase = true) == true)
        }
        // Recently-Played shelf hides entirely when the rail toggle is off (#263);
        // emptying the list here also drops its header, controller row, and the
        // "Library" section label — leaving one unified library. It also hides
        // while a search filter is active, so results present as one grid.
        val currentShelfGames = if (listMode || searchQ.isNotEmpty() || !LibraryRecentShelf.show.value) emptyList() else recentUris
            .mapNotNull { uri -> games.firstOrNull { it.uri.toString() == uri } }
            .take(perRow)
            .ifEmpty { games.take(perRow) }
        val libraryRows = visibleGames.chunked(perRow)
        val controllerLayoutRows = buildList {
            if (currentShelfGames.isNotEmpty()) add(currentShelfGames)
            addAll(libraryRows)
        }
        val firstLibraryRowItem = if (currentShelfGames.isNotEmpty()) 3 else 1
        val controllerRowsForUi = buildList {
            if (currentShelfGames.isNotEmpty()) add(ControllerGameRow(0, currentShelfGames, 1))
            libraryRows.forEachIndexed { index, row ->
                add(ControllerGameRow(index + 1, row, firstLibraryRowItem + index))
            }
        }
        val controllerVisibleKey = controllerLayoutRows
            .joinToString("||") { row -> row.joinToString("|") { it.uri.toString() } }
        val listState = rememberLazyListState()
        val density = LocalDensity.current

        LaunchedEffect(controllerVisibleKey, landscape) {
            updateControllerLayout(controllerRowsForUi)
        }

        LaunchedEffect(controllerSelectedUri.value, controllerVisibleKey) {
            controllerSelectedListItemIndex()?.let { index ->
                val visible = listState.layoutInfo.visibleItemsInfo
                val first = visible.firstOrNull()?.index
                val last = visible.lastOrNull()?.index
                when {
                    first == null || last == null -> listState.scrollToItem(index)
                    index < first -> listState.scrollToItem(index)
                    index > last -> {
                        val visibleSpan = (last - first).coerceAtLeast(0)
                        listState.scrollToItem((index - visibleSpan).coerceAtLeast(0))
                    }
                }
            }
        }

        // Bring the toolbar into view when the controller moves up into it; the
        // gear rail is always on-screen so it needs no scroll.
        LaunchedEffect(controllerZone.value) {
            if (controllerZone.value == Zone.TOOLBAR) listState.scrollToItem(0)
        }
        // Start library browsing on the game grid each time it's explicitly shown.
        LaunchedEffect(WindowImpl.showLibrary.value) {
            if (WindowImpl.showLibrary.value) resetControllerZone()
        }
        LaunchedEffect(listState) {
            var lastFrame = withFrameNanos { it }
            while (true) {
                val frame = withFrameNanos { it }
                val dt = ((frame - lastFrame).coerceAtMost(50_000_000L)).toFloat() / 1_000_000_000f
                lastFrame = frame
                val velocity = controllerScrollVelocity.value
                if (abs(velocity) > 0.08f) {
                    val pxPerSecond = with(density) { 1700.dp.toPx() }
                    listState.scrollBy(velocity * pxPerSecond * dt)
                }
            }
        }

        Box(modifier = modifier.fillMaxSize()) {
            val customBackground = customBackgroundPath.value
                ?.let { File(it) }
                ?.takeIf { it.exists() && it.length() > 0L }
            if (customBackground != null) {
                SubcomposeAsyncImage(
                    model = ImageRequest.Builder(context)
                        .data(customBackground)
                        .crossfade(true)
                        .build(),
                    contentDescription = null,
                    modifier = Modifier.matchParentSize(),
                    contentScale = ContentScale.Crop,
                    alignment = Alignment.Center,
                )
                Box(
                    Modifier
                        .matchParentSize()
                        .background(Color.Black.copy(alpha = 0.38f)),
                )
                Box(
                    Modifier
                        .matchParentSize()
                        .background(
                            Brush.verticalGradient(
                                listOf(Color.Black.copy(alpha = 0.10f), Color.Black.copy(alpha = 0.42f)),
                            ),
                        ),
                )
            } else {
                // Base reactor-blue wash (unchanged palette).
                Box(
                    Modifier
                        .matchParentSize()
                        .background(
                            Brush.radialGradient(
                                listOf(
                                    Color(0xFF07355E),
                                    Color(0xFF001C35),
                                    Color(0xFF000814),
                                ),
                            ),
                        ),
                )
                // "Inside the core reactor": the app's glowing monolith stands
                // tall behind the shelves, scaled past the screen height and
                // dimmed so it reads as ambient light rather than a foreground object.
                Image(
                    painter = painterResource(id = R.drawable.savetowerforeground),
                    contentDescription = null,
                    modifier = Modifier
                        .align(Alignment.Center)
                        .fillMaxHeight(1.25f)
                        .graphicsLayer { alpha = 0.30f },
                    contentScale = ContentScale.Fit,
                )
                // Vertical vignette so the tower glow falls off toward the bottom and
                // the shelf rows keep their contrast.
                Box(
                    Modifier
                        .matchParentSize()
                        .background(
                            Brush.verticalGradient(
                                listOf(Color.Transparent, Color(0x66000814)),
                            ),
                        ),
                )
            }
            LazyColumn(
                state = listState,
                modifier = Modifier
                    .fillMaxSize()
                    .padding(
                        horizontal = if (landscape) 26.dp else 18.dp,
                        vertical = if (landscape) 22.dp else 20.dp,
                    ),
                verticalArrangement = Arrangement.spacedBy(if (landscape) 18.dp else 16.dp),
                contentPadding = PaddingValues(bottom = 18.dp),
            ) {
            if (currentShelfGames.isNotEmpty()) {
                item(key = "__current_title__") {
                    HeaderRow(
                        title = str("games.section.recentlyPlayed"),
                        context = context,
                        romsDirs = romsDirs,
                        romsKey = romsKey,
                    )
                }
                item(key = "__current_shelf__") {
                        val recentCoverW = customCoverW ?: (if (landscape) 92.dp else 98.dp)
                        val titlesExtra = if (LibraryTitles.show.value) 44.dp else 0.dp
                        val recentShelfH = if (customCoverW != null)
                            ((recentCoverW.value / 0.7f) + 84f).dp + titlesExtra
                        else (if (landscape) 220.dp else 194.dp) + titlesExtra
                        GameShelf(
                            games = currentShelfGames,
                            label = null,
                            rowId = 0,
                            listItemIndex = 1,
                            coverWidth = recentCoverW,
                            nowPlaying = true, // vivi's Now Playing shelf here only
                            modifier = Modifier
                                .fillMaxWidth()
                                // Grow the shelf when titles are on so the 2-line
                                // label + version tag below the cover isn't clipped.
                                .height(recentShelfH),
                        )
                }
            } else {
                item(key = "__empty_actions__") {
                    HeaderRow(
                        title = str("games.section.library"),
                        context = context,
                        romsDirs = romsDirs,
                        romsKey = romsKey,
                    )
                }
            }

            if (games.isNotEmpty()) {
                // Only label the "Library" section when a "Recently Played" shelf sits
                // above it; otherwise the top HeaderRow already reads "Library" (avoids
                // the doubled heading in list view / no-recent libraries).
                if (currentShelfGames.isNotEmpty()) {
                    item(key = "__library_title__") {
                        SectionHeader(str("games.section.library"))
                    }
                }
                lazyItemsIndexed(libraryRows, key = { _, row ->
                    row.joinToString("|") { it.uri.toString() }
                }) { rowIndex, row ->
                    if (listMode) {
                        // List view: one full-width name row per game (perRow == 1).
                        ListGameRow(game = row.first(), rowId = rowIndex + 1)
                    } else {
                        val label = shelfLabelFor(row)
                        val libCoverW = customCoverW ?: (if (landscape) 86.dp else 98.dp)
                        val titlesExtra = if (LibraryTitles.show.value) 44.dp else 0.dp
                        val libShelfH = if (customCoverW != null)
                            ((libCoverW.value / 0.7f) + 84f).dp + titlesExtra
                        else (if (landscape) 204.dp else 230.dp) + titlesExtra
                        GameShelf(
                            games = row,
                            label = label,
                            rowId = rowIndex + 1,
                            listItemIndex = firstLibraryRowItem + rowIndex,
                            coverWidth = libCoverW,
                            modifier = Modifier
                                .fillMaxWidth()
                                // Grow the shelf when titles are on so the 2-line
                                // label + version tag below the cover isn't clipped.
                                .height(libShelfH),
                        )
                    }
                }
            }
            }
            // #267: gamepad-first search keyboard, floating over the grid so the
            // filtered results stay visible and update live as keys are pressed.
            if (searchOpen.value) SearchPanel(resultCount = visibleGames.size)
        }
    }

    /** RetroArch-style on-screen search keyboard: D-pad moves the focused key,
     *  A presses it, B closes-and-clears, "Done" closes keeping the filter. Every
     *  key is also tappable for touch users. Anchored to the bottom so the
     *  filtered shelves remain visible above it. */
    @Composable
    private fun androidx.compose.foundation.layout.BoxScope.SearchPanel(resultCount: Int) {
        Column(
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .fillMaxWidth()
                .background(
                    Brush.verticalGradient(
                        listOf(Color(0xF20A1626), Color(0xF7060D18)),
                    ),
                )
                .padding(horizontal = 10.dp, vertical = 8.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(5.dp),
        ) {
            Text(
                (if (searchQuery.value.isEmpty()) str("games.search.placeholder") else searchQuery.value) +
                    "   •   $resultCount found",
                color = Color.White,
                fontFamily = TitleFont,
                fontSize = 15.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            searchKeys.forEachIndexed { r, row ->
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    row.forEachIndexed { c, key ->
                        val focused = searchKbRow.value == r &&
                            searchKbCol.value.coerceIn(0, row.lastIndex) == c
                        Box(
                            Modifier
                                .defaultMinSize(
                                    minWidth = if (key.length > 1) 52.dp else 30.dp,
                                    minHeight = 30.dp,
                                )
                                .background(
                                    if (focused) Color(0xFF2E6BB0) else Color(0x26FFFFFF),
                                    RoundedCornerShape(5.dp),
                                )
                                .clickable {
                                    searchKbRow.value = r
                                    searchKbCol.value = c
                                    searchKeyPress(key)
                                }
                                .padding(horizontal = 6.dp),
                            contentAlignment = Alignment.Center,
                        ) {
                            Text(key, color = Color.White, fontSize = 13.sp)
                        }
                    }
                }
            }
            Text(
                str("games.search.hint"),
                color = Color.White.copy(alpha = 0.5f),
                fontSize = 10.sp,
            )
        }
    }

    @Composable
    private fun HeaderRow(
        title: String,
        context: Context,
        romsDirs: List<String>,
        romsKey: String,
    ) {
        // Boot ELF picker. Copies the chosen .elf into a real file under the
        // app data dir and boots it via the normal launch path — emucore's
        // AutoDetectSource recognizes the .elf extension (s_elf_override,
        // NoDisc), so homebrew / HDD-loader ELFs boot through the BIOS. We copy
        // (rather than pass the content URI) so the .elf suffix and a plain
        // file path are guaranteed regardless of the document provider.
        val bootElfLauncher = rememberLauncherForActivityResult(
            contract = ActivityResultContracts.OpenDocument()
        ) { uri ->
            if (uri == null) return@rememberLauncherForActivityResult
            val name = DocumentFile.fromSingleUri(context, uri)?.name
                ?.takeIf { it.isNotEmpty() } ?: "boot.elf"
            val outName = if (name.endsWith(".elf", ignoreCase = true)) name else "$name.elf"
            val outFile = File(File(Main.assetCopyRoot(context), "elf").apply { mkdirs() }, outName)
            val copied = runCatching {
                context.contentResolver.openInputStream(uri)?.use { ins ->
                    outFile.outputStream().use { outs -> ins.copyTo(outs) }
                } != null
            }.getOrDefault(false)
            if (copied && outFile.length() > 0L) {
                WindowImpl.showLibrary.value = false
                // Carry a GameInfo (serial-less, keyed by the ELF filename stem)
                // so the ELF's per-game settings resolve at boot instead of
                // falling back to global (issue #253). Without this, currentGame
                // is null and the boot path reads only the global tier.
                val elfInfo = com.armsx2.GameInfo(
                    uri = android.net.Uri.fromFile(outFile),
                    title = outName.substringBeforeLast('.'),
                    serial = null,
                    extension = "ELF",
                )
                Main.launchGame(outFile.absolutePath, elfInfo)
            }
        }
        val wallLauncher = rememberLauncherForActivityResult(
            contract = ActivityResultContracts.OpenDocument()
        ) { uri ->
            if (uri != null)
                importCustomBackground(context, uri)
        }

        // Single source of truth for the toolbar so the rendered chips, the
        // controller highlight and the confirm action all stay in sync. Built
        // here in composition so the closures capture the live launchers/context;
        // published to the nav model via SideEffect.
        val toolbarExpanded = remember { mutableStateOf(false) }
        val showExitConfirm = remember { mutableStateOf(false) }

        // (icon, caption, action). The first four are always visible; the rest fold
        // behind the "⋮ More" toggle. Each caption renders UNDER its icon. Controller
        // nav sees a flat list that grows/shrinks when the toggle flips.
        val toolbarActions: List<Triple<String, String, () -> Unit>> = buildList {
            // #267: search/filter the library — gamepad-first (Y also opens it).
            // Caption reflects an active filter so it's obvious why games "vanished".
            add(Triple(
                LibIcons.SEARCH,
                if (searchQuery.value.isEmpty()) str("games.toolbar.search") else "\"${searchQuery.value.take(8)}\"",
            ) { openSearch() })
            add(Triple(LibIcons.REFRESH, str("games.toolbar.scan")) {
                when {
                    romsDirs.isEmpty() ->
                        Toast.makeText(context, I18n.get("games.toast.chooseFolderFirst"), Toast.LENGTH_SHORT).show()
                    scanning.value ->
                        Toast.makeText(context, I18n.get("games.toast.scanAlreadyRunning"), Toast.LENGTH_SHORT).show()
                    else -> {
                        Toast.makeText(context, I18n.get("games.toast.scanningLibrary"), Toast.LENGTH_SHORT).show()
                        lastScannedRoms.value = null
                        scanRoms(context, romsDirs, romsKey)
                    }
                }
            })
            add(Triple(LibIcons.CPU, str("games.toolbar.bios")) {
                WindowImpl.showLibrary.value = false
                Main.startBios()
            })
            add(Triple(LibIcons.SD_CARD, str("games.toolbar.cards")) { MemoryCardManager.visible.value = true })
            add(Triple(
                if (CoverArtStyle.use3d.value) LibIcons.BOX else LibIcons.PHOTO,
                if (CoverArtStyle.use3d.value) str("games.toolbar.cover3d") else str("games.toolbar.cover2d"),
            ) { CoverArtStyle.set(!CoverArtStyle.use3d.value) })
            add(Triple(LibIcons.DOTS, if (toolbarExpanded.value) str("games.toolbar.less") else str("games.toolbar.more")) {
                toolbarExpanded.value = !toolbarExpanded.value
            })
            if (toolbarExpanded.value) {
                add(Triple(LibIcons.FILE_CODE, str("games.toolbar.elf")) { bootElfLauncher.launch(arrayOf("*/*")) })
                add(Triple(LibIcons.WALLPAPER, str("games.toolbar.background")) { wallLauncher.launch(arrayOf("image/*")) })
                if (customBackgroundPath.value != null) {
                    add(Triple(LibIcons.REFRESH, str("games.toolbar.resetBg")) { resetCustomBackground(context) })
                }
                // Cover-grid size (shelf view only): cycle columns / rows; cover size
                // adjusts to fit. Captions show the live value (Auto = auto-fit).
                // Rows control only (covers auto-fit the width). The Columns control
                // was removed — fewer columns zoomed covers up, which hurt on small
                // screens; Rows (cover height / how many fit vertically) is the useful knob.
                if (!LibraryView.listMode.value) {
                    add(Triple(
                        LibIcons.LAYOUT_ROWS,
                        str("games.toolbar.rows") + " " + (if (LibraryView.rows.value == 0) str("games.toolbar.auto") else LibraryView.rows.value.toString()),
                    ) { LibraryView.cycleRows() })
                }
                // Recently-Played shelf on/off — lives up here in the top toolbar next
                // to Rows so it doesn't crowd the settings gear off the left rail.
                add(Triple(
                    LibIcons.CLOCK,
                    str("games.toolbar.recent") + " " + (if (LibraryRecentShelf.show.value) str("games.toolbar.on") else str("games.toolbar.off")),
                ) { LibraryRecentShelf.set(!LibraryRecentShelf.show.value) })
                add(Triple(LibIcons.TOOL, str("games.toolbar.setup")) {
                    SetupImpl.resetForReentry()
                    Main.reopenSetup()
                })
                add(Triple(LibIcons.POWER, str("games.toolbar.exit")) { showExitConfirm.value = true })
            }
        }
        SideEffect { controllerToolbarActions = toolbarActions.map { it.second to it.third } }
        val toolbarFocusIndex =
            if (controllerZone.value == Zone.TOOLBAR) controllerToolbarIndex.value else -1

        Row(
            Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(3.dp),
        ) {
            SectionHeader(
                text = title,
                modifier = Modifier.weight(1f),
            )
            toolbarActions.forEachIndexed { idx, (icon, label, action) ->
                IconActionChip(
                    iconPath = icon,
                    label = label,
                    highlighted = idx == toolbarFocusIndex,
                    onClick = action,
                )
            }
        }
        if (showExitConfirm.value) {
            androidx.compose.material3.AlertDialog(
                onDismissRequest = { showExitConfirm.value = false },
                containerColor = Color(0xFF1B1A1A),
                title = { Text(str("games.exit.title"), color = Color.White) },
                text = { Text(str("games.exit.message"), color = Color(0xFFCCCCCC)) },
                confirmButton = {
                    androidx.compose.material3.TextButton(onClick = {
                        showExitConfirm.value = false
                        Main.exitApp()
                    }) { Text(str("action.yes"), color = Color(0xFFFF6B6B)) }
                },
                dismissButton = {
                    androidx.compose.material3.TextButton(onClick = { showExitConfirm.value = false }) {
                        Text(str("action.no"), color = Color(0xFFAACCFF))
                    }
                },
            )
        }
    }

    /** Tabler-style outline icon (MIT, github.com/tabler/tabler-icons) drawn from
     *  its 24x24 SVG path data. Stroked to match the app's hand-drawn glyphs — no
     *  icon-font / Material-icons dependency, so the (un-minified) APK stays lean. */
    @Composable
    private fun TablerIcon(pathData: String, modifier: Modifier = Modifier, tint: Color = Color.White) {
        val path = remember(pathData) {
            try {
                PathParser().parsePathString(pathData).toPath()
            } catch (_: Exception) {
                Path()
            }
        }
        Canvas(modifier) {
            val unit = size.minDimension / 24f
            scale(unit, unit, pivot = Offset.Zero) {
                drawPath(
                    path,
                    color = tint,
                    style = Stroke(width = 2f, cap = StrokeCap.Round, join = StrokeJoin.Round),
                )
            }
        }
    }

    /** Toolbar button: a Tabler icon with a short caption underneath (KamFretoZ's
     *  request — clearer than a single side label). Tap runs the action; the
     *  controller-focus glow sits on the icon pill. */
    @Composable
    private fun IconActionChip(
        iconPath: String,
        label: String,
        highlighted: Boolean,
        onClick: () -> Unit,
    ) {
        val glow = Color(0xFF3DA5FF)
        Column(
            Modifier
                .clip(RoundedCornerShape(10.dp))
                .clickable(onClick = onClick)
                .padding(horizontal = 2.dp, vertical = 2.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Box(
                Modifier
                    .height(34.dp)
                    .widthIn(min = 40.dp)
                    .then(
                        if (highlighted)
                            Modifier.shadow(10.dp, RoundedCornerShape(17.dp), ambientColor = glow, spotColor = glow)
                        else Modifier
                    )
                    .clip(RoundedCornerShape(17.dp))
                    .background(Color.White.copy(alpha = if (highlighted) 0.20f else 0.10f))
                    .border(
                        1.dp,
                        if (highlighted) glow else Color.White.copy(alpha = 0.18f),
                        RoundedCornerShape(17.dp),
                    ),
                contentAlignment = Alignment.Center,
            ) {
                TablerIcon(iconPath, Modifier.size(20.dp), Color.White)
            }
            Spacer(Modifier.height(3.dp))
            Text(
                label,
                color = if (highlighted) Color.White else Color.White.copy(alpha = 0.75f),
                fontSize = 10.sp,
                fontWeight = FontWeight.Medium,
                maxLines = 1,
            )
        }
    }

    /** Outline icon path data (24x24 viewBox), from Tabler Icons (MIT). */
    private object LibIcons {
        const val SEARCH = "M3 10a7 7 0 1 0 14 0a7 7 0 1 0 -14 0 M21 21l-6 -6"
        const val CPU = "M5 6a1 1 0 0 1 1 -1h12a1 1 0 0 1 1 1v12a1 1 0 0 1 -1 1h-12a1 1 0 0 1 -1 -1l0 -12 M9 9h6v6h-6l0 -6 M3 10h2 M3 14h2 M10 3v2 M14 3v2 M21 10h-2 M21 14h-2 M14 21v-2 M10 21v-2"
        const val SD_CARD = "M7 21h10a2 2 0 0 0 2 -2v-14a2 2 0 0 0 -2 -2h-6.172a2 2 0 0 0 -1.414 .586l-3.828 3.828a2 2 0 0 0 -.586 1.414v10.172a2 2 0 0 0 2 2 M13 6v2 M16 6v2 M10 7v1"
        const val BOX = "M12 3l8 4.5l0 9l-8 4.5l-8 -4.5l0 -9l8 -4.5 M12 12l8 -4.5 M12 12l0 9 M12 12l-8 -4.5"
        const val PHOTO = "M15 8h.01 M3 6a3 3 0 0 1 3 -3h12a3 3 0 0 1 3 3v12a3 3 0 0 1 -3 3h-12a3 3 0 0 1 -3 -3v-12 M3 16l5 -5c.928 -.893 2.072 -.893 3 0l5 5 M14 14l1 -1c.928 -.893 2.072 -.893 3 0l3 3"
        const val WALLPAPER = "M8 6h10a2 2 0 0 1 2 2v10a2 2 0 0 1 -2 2h-12 M4 18a2 2 0 1 0 4 0a2 2 0 1 0 -4 0 M8 18v-12a2 2 0 1 0 -4 0v12"
        const val DOTS = "M11 12a1 1 0 1 0 2 0a1 1 0 1 0 -2 0 M11 19a1 1 0 1 0 2 0a1 1 0 1 0 -2 0 M11 5a1 1 0 1 0 2 0a1 1 0 1 0 -2 0"
        const val FILE_CODE = "M14 3v4a1 1 0 0 0 1 1h4 M17 21h-10a2 2 0 0 1 -2 -2v-14a2 2 0 0 1 2 -2h7l5 5v11a2 2 0 0 1 -2 2 M10 13l-1 2l1 2 M14 13l1 2l-1 2"
        const val REFRESH = "M20 11a8.1 8.1 0 0 0 -15.5 -2m-.5 -4v4h4 M4 13a8.1 8.1 0 0 0 15.5 2m.5 4v-4h-4"
        const val TOOL = "M7 10h3v-3l-3.5 -3.5a6 6 0 0 1 8 8l6 6a2 2 0 0 1 -3 3l-6 -6a6 6 0 0 1 -8 -8l3.5 3.5"
        // Tabler "power" — reads as quit/close-the-app.
        const val POWER = "M7 6a7.75 7.75 0 1 0 10 0 M12 4l0 8"
        const val LAYOUT_COLUMNS = "M5 4h14a1 1 0 0 1 1 1v14a1 1 0 0 1 -1 1h-14a1 1 0 0 1 -1 -1v-14a1 1 0 0 1 1 -1 M12 4v16"
        const val LAYOUT_ROWS = "M5 4h14a1 1 0 0 1 1 1v14a1 1 0 0 1 -1 1h-14a1 1 0 0 1 -1 -1v-14a1 1 0 0 1 1 -1 M4 12h16"
        // Tabler "clock" — the Recently-Played shelf toggle.
        const val CLOCK = "M3 12a9 9 0 1 0 18 0a9 9 0 0 0 -18 0 M12 7v5l3 3"

        // vivi's settings cog — NOTE a 95x95 viewBox (unlike the 24x24 Tabler glyphs
        // above), so NavGlyph scales it by /95. Two subpaths (center-hole outline +
        // outer gear outline); stroked to read as an outline glyph like the rest of the rail.
        const val SETTINGS_COG = "M71.1113 47.5C71.1113 34.4607 60.5393 23.8887 47.5 23.8887C34.4607 23.8887 23.8887 34.4607 23.8887 47.5C23.8887 60.5393 34.4607 71.1113 47.5 71.1113C60.5393 71.1113 71.1113 60.5393 71.1113 47.5ZM14.1494 42.0859C15.3082 42.0859 16.315 41.2896 16.582 40.1621C17.3742 36.815 18.6925 33.6806 20.4512 30.8359C21.0228 29.9113 20.928 28.7315 20.2383 27.9121L20.0918 27.7539L13.8154 21.4766C12.7313 20.391 12.7331 18.6292 13.8145 17.5479L17.5459 13.8174C18.6314 12.7319 20.3944 12.7332 21.4756 13.8145L27.7539 20.0918C28.5734 20.9113 29.8488 21.061 30.835 20.4521C33.6814 18.6949 36.8157 17.3739 40.1592 16.582C41.2162 16.3317 41.9826 15.4316 42.0742 14.3652L42.083 14.1494L42.083 5.27735C42.0832 3.74492 43.3289 2.5 44.8613 2.5L50.1387 2.5C51.6712 2.5 52.9168 3.74493 52.917 5.27735L52.917 14.1494C52.917 15.3082 53.7133 16.315 54.8408 16.582C58.1853 17.3742 61.3196 18.6926 64.165 20.4492C65.1511 21.058 66.4266 20.9091 67.2461 20.0898L73.5244 13.8115C74.6092 12.7294 76.3706 12.7291 77.4551 13.8135L81.1856 17.5479L81.1865 17.5498C82.269 18.631 82.2708 20.3903 81.1856 21.4756L81.1865 21.4756L74.9082 27.75C74.0884 28.5695 73.939 29.8457 74.5479 30.832C76.3051 33.6784 77.6261 36.8131 78.418 40.1592C78.6849 41.2868 79.6917 42.083 80.8506 42.083L89.7227 42.083C91.2551 42.0832 92.5 43.3289 92.5 44.8613L92.5 50.1387C92.5 51.6712 91.2551 52.9168 89.7227 52.917L80.8506 52.917C79.6917 52.917 78.6849 53.7132 78.418 54.8408C77.6259 58.1874 76.3082 61.3199 74.5508 64.1689C73.9426 65.155 74.0911 66.4297 74.9102 67.249L81.1885 73.5273C82.2729 74.612 82.2729 76.3695 81.1885 77.4541L77.457 81.1856C76.3724 82.2702 74.612 82.2702 73.5273 81.1856L67.249 74.9082C66.4295 74.0887 65.1541 73.9391 64.168 74.5479C61.3216 76.3051 58.1869 77.6261 54.8408 78.418C53.7132 78.6849 52.917 79.6917 52.917 80.8506L52.917 89.7227C52.9168 91.2559 51.6706 92.5 50.1416 92.5L44.8633 92.5C43.331 92.4997 42.0862 91.2549 42.0859 89.7227L42.0859 80.8506C42.0859 79.764 41.386 78.8103 40.3691 78.4756L40.1621 78.418C36.815 77.6258 33.6806 76.3075 30.8359 74.5488C29.8496 73.9391 28.5738 74.0883 27.7539 74.9082L21.4756 81.1855C20.3909 82.2702 18.6306 82.2702 17.5459 81.1855L13.8145 77.4541C12.7298 76.3695 12.7299 74.612 13.8145 73.5273L20.0918 67.249C20.9113 66.4295 21.0609 65.1541 20.4521 64.168C18.6948 61.3215 17.3739 58.1864 16.582 54.8428C16.3148 53.7154 15.308 52.9189 14.1494 52.9189L5.27735 52.9189C3.74495 52.9187 2.50004 51.6741 2.5 50.1416L2.5 44.8633C2.50026 43.331 3.74508 42.0862 5.27735 42.0859L14.1494 42.0859Z"

        // vivi's "Now Playing" shelf — two layered translucent trapezoids
        // (2850x159 viewBox, blue #4B83D7 @ 0.2), drawn behind the Recently
        // Played covers as a ledge they sit on. NowPlayingShelf scales the viewBox.
        const val SHELF_LOWER = "M180.283 23.2378C180.574 23.0817 180.899 23 181.229 23H2643.85C2644.15 23 2644.44 23.0656 2644.71 23.1921L2788.97 91.4408C2803.44 98.2858 2798.56 120 2782.56 120H59.6655C44.0712 120 38.8316 99.1581 52.5719 91.7834L180.283 23.2378Z"
        const val SHELF_UPPER = "M180.283 11.2378C180.574 11.0817 180.899 11 181.229 11H2643.85C2644.15 11 2644.44 11.0656 2644.71 11.1921L2788.97 79.4408C2803.44 86.2858 2798.56 108 2782.56 108H59.6655C44.0712 108 38.8316 87.1581 52.5719 79.7834L180.283 11.2378Z"
    }

    @Composable
    private fun LibraryNavRail(landscape: Boolean, modifier: Modifier = Modifier) {
        if (landscape) {
            Column(
                modifier
                    .fillMaxHeight()
                    .width(82.dp)
                    .background(
                        Brush.verticalGradient(
                            listOf(Color(0xFF06416E), Color(0xFF002C50)),
                        ),
                    )
                    .padding(vertical = 24.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.SpaceBetween,
            ) {
                NavButton(NavKind.Library, str("games.nav.library"), active = true) {}
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.spacedBy(14.dp),
                ) {
                    InfoButton(
                        highlighted = controllerZone.value == Zone.RAIL && railSelection.value == 0,
                    ) { infoDialogOpen.value = true }
                    TitlesToggleButton(
                        highlighted = controllerZone.value == Zone.RAIL && railSelection.value == 1,
                    )
                    ListViewToggleButton(
                        highlighted = controllerZone.value == Zone.RAIL && railSelection.value == 2,
                    )
                }
                NavButton(
                    NavKind.Settings,
                    null,
                    active = false,
                    highlighted = controllerZone.value == Zone.RAIL && railSelection.value == 3,
                ) { InGameOverlay.openGlobalSettings() }
            }
        } else {
            Row(
                modifier
                    .fillMaxWidth()
                    .height(82.dp)
                    .background(
                        Brush.verticalGradient(
                            listOf(Color(0xCC0E4B79), Color(0xEE002E52)),
                        ),
                    )
                    .padding(horizontal = 28.dp, vertical = 8.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                NavButton(NavKind.Library, str("games.nav.library"), active = true) {}
                Row(
                    horizontalArrangement = Arrangement.spacedBy(14.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    InfoButton(
                        highlighted = controllerZone.value == Zone.RAIL && railSelection.value == 0,
                    ) { infoDialogOpen.value = true }
                    TitlesToggleButton(
                        highlighted = controllerZone.value == Zone.RAIL && railSelection.value == 1,
                    )
                    ListViewToggleButton(
                        highlighted = controllerZone.value == Zone.RAIL && railSelection.value == 2,
                    )
                }
                NavButton(
                    NavKind.Settings,
                    null,
                    active = false,
                    highlighted = controllerZone.value == Zone.RAIL && railSelection.value == 3,
                ) { InGameOverlay.openGlobalSettings() }
            }
        }
    }

    @Composable
    private fun NavButton(
        kind: NavKind,
        label: String?,
        active: Boolean,
        highlighted: Boolean = false,
        onClick: () -> Unit,
    ) {
        val glow = Color(0xFF3DA5FF)
        Column(
            Modifier
                .width(70.dp)
                .clip(RoundedCornerShape(16.dp))
                .clickable(onClick = onClick)
                .padding(vertical = 6.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Box(
                Modifier
                    .size(42.dp)
                    .then(
                        if (highlighted)
                            Modifier.shadow(10.dp, CircleShape, ambientColor = glow, spotColor = glow)
                        else Modifier
                    )
                    .clip(if (active) RoundedCornerShape(12.dp) else CircleShape)
                    .background(
                        when {
                            highlighted -> glow.copy(alpha = 0.30f)
                            active -> Color.White.copy(alpha = 0.10f)
                            else -> Color.Transparent
                        }
                    )
                    .then(
                        if (highlighted) Modifier.border(1.dp, glow, CircleShape) else Modifier
                    ),
                contentAlignment = Alignment.Center,
            ) {
                NavGlyph(kind, active)
            }
            if (label != null) {
                Text(
                    label,
                    color = Color.White,
                    fontSize = 11.sp,
                    fontWeight = FontWeight.Bold,
                    maxLines = 1,
                )
            }
        }
    }

    private enum class NavKind { Library, Settings }

    /** Compact ⓘ button on the rail. Opens [InfoScreen] with all the library
     *  help text — kept off the rail itself so it can't push the gear
     *  off-screen on short displays. */
    @Composable
    private fun InfoButton(highlighted: Boolean, onClick: () -> Unit) {
        val glow = Color(0xFF3DA5FF)
        Box(
            Modifier
                .size(42.dp)
                .then(
                    if (highlighted)
                        Modifier.shadow(10.dp, CircleShape, ambientColor = glow, spotColor = glow)
                    else Modifier
                )
                .clip(CircleShape)
                .background(
                    if (highlighted) glow.copy(alpha = 0.30f) else Color.White.copy(alpha = 0.10f)
                )
                .then(if (highlighted) Modifier.border(1.dp, glow, CircleShape) else Modifier)
                .clickable(onClick = onClick),
            contentAlignment = Alignment.Center,
        ) {
            Text(
                "i",
                color = Color.White,
                fontSize = 20.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = androidx.compose.ui.text.font.FontFamily.Serif,
            )
        }
    }

    /** Left-rail toggle (under the info button) for showing game titles under
     *  the shelf covers. "Aa" lights up when on; tap flips [LibraryTitles] live.
     *  [highlighted] draws the controller-focus ring (independent of on/off). */
    @Composable
    private fun TitlesToggleButton(highlighted: Boolean = false) {
        val on = LibraryTitles.show.value
        val glow = Color(0xFF3DA5FF)
        Box(
            Modifier
                .size(42.dp)
                .then(
                    if (highlighted)
                        Modifier.shadow(10.dp, CircleShape, ambientColor = glow, spotColor = glow)
                    else Modifier
                )
                .clip(CircleShape)
                .background(if (on || highlighted) glow.copy(alpha = 0.30f) else Color.White.copy(alpha = 0.10f))
                .then(if (on || highlighted) Modifier.border(1.dp, glow, CircleShape) else Modifier)
                .clickable { LibraryTitles.set(!on) },
            contentAlignment = Alignment.Center,
        ) {
            Text(
                "Aa",
                color = if (on || highlighted) Color.White else Color.White.copy(alpha = 0.55f),
                fontSize = 15.sp,
                fontWeight = FontWeight.Bold,
            )
        }
    }

    /** Rail button under "Aa": toggle between the cover SHELF view and the compact
     *  LIST (names-only) view. Filled when list view is active. Long-press cycles
     *  the shelf grid columns (cover size) for quick tuning without a menu. */
    @Composable
    private fun ListViewToggleButton(highlighted: Boolean = false) {
        val listOn = LibraryView.listMode.value
        val glow = Color(0xFF3DA5FF)
        Box(
            Modifier
                .size(42.dp)
                .then(
                    if (highlighted)
                        Modifier.shadow(10.dp, CircleShape, ambientColor = glow, spotColor = glow)
                    else Modifier
                )
                .clip(CircleShape)
                .background(if (listOn || highlighted) glow.copy(alpha = 0.30f) else Color.White.copy(alpha = 0.10f))
                .then(if (listOn || highlighted) Modifier.border(1.dp, glow, CircleShape) else Modifier)
                .clickable { LibraryView.toggleListMode() },
            contentAlignment = Alignment.Center,
        ) {
            // ☰ = list view active; ▦ = cover shelf view active.
            Text(
                if (listOn) "☰" else "▦",
                color = if (listOn || highlighted) Color.White else Color.White.copy(alpha = 0.55f),
                fontSize = 17.sp,
                fontWeight = FontWeight.Bold,
            )
        }
    }

    /** Help text shown when the rail's info button is tapped. Full-screen scrim
     *  + a scrollable card so it fits the smallest displays. Tap anywhere (or
     *  press B / back on a controller) to dismiss. */
    @Composable
    private fun InfoScreen(onDismiss: () -> Unit) {
        Box(
            Modifier
                .fillMaxSize()
                .background(Color(0xE6000A14))
                .clickable(onClick = onDismiss),
            contentAlignment = Alignment.Center,
        ) {
            Column(
                Modifier
                    .widthIn(max = 460.dp)
                    .heightIn(max = 360.dp)
                    .padding(horizontal = 24.dp)
                    .verticalScroll(rememberScrollState()),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Text(
                    str("games.info.title"),
                    color = Colors.pasx2_blue,
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold,
                )
                Spacer(Modifier.height(16.dp))
                InfoParagraph(
                    str("games.info.navigating.title"),
                    str("games.info.navigating.body"),
                )
                InfoParagraph(
                    str("games.info.perGameSettings.title"),
                    str("games.info.perGameSettings.body"),
                )
                InfoParagraph(
                    str("games.info.inGameMenu.title"),
                    str("games.info.inGameMenu.body"),
                )
                // Open / copy the app data folder (memory cards, custom textures, etc.).
                val ctx = LocalContext.current
                val dataPath = remember {
                    Main.systemDirPosix() ?: ctx.getExternalFilesDir(null)?.absolutePath ?: ""
                }
                Column(
                    Modifier
                        .fillMaxWidth()
                        .padding(bottom = 14.dp)
                        .clip(RoundedCornerShape(8.dp))
                        .background(Color(0xFF16202C))
                        .clickable { openOrCopyDataFolder(ctx) }
                        .padding(12.dp),
                ) {
                    Text(
                        str("games.info.openDataFolder.title"),
                        color = Colors.pasx2_blue,
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Bold,
                    )
                    Spacer(Modifier.height(4.dp))
                    Text(
                        str("games.info.openDataFolder.body"),
                        color = Color.White.copy(alpha = 0.72f),
                        fontSize = 12.sp,
                        lineHeight = 16.sp,
                    )
                    if (dataPath.isNotEmpty()) {
                        Spacer(Modifier.height(6.dp))
                        Text(dataPath, color = Color.White.copy(alpha = 0.5f), fontSize = 10.sp)
                    }
                }
                Spacer(Modifier.height(18.dp))
                Text(
                    str("games.info.tapToClose"),
                    color = Color.White.copy(alpha = 0.5f),
                    fontSize = 11.sp,
                    fontWeight = FontWeight.SemiBold,
                )
                Spacer(Modifier.height(8.dp))
            }
        }
    }

    @Composable
    private fun InfoParagraph(title: String, body: String) {
        Column(Modifier.fillMaxWidth().padding(bottom = 14.dp)) {
            Text(
                title,
                color = Color.White,
                fontSize = 14.sp,
                fontWeight = FontWeight.Bold,
            )
            Spacer(Modifier.height(4.dp))
            Text(
                body,
                color = Color.White.copy(alpha = 0.72f),
                fontSize = 12.sp,
                lineHeight = 16.sp,
            )
        }
    }

    /** Best-effort: open the app data folder in a file manager; if nothing can
     *  handle it (common on newer Android for Android/data), copy the path to
     *  the clipboard so the user can paste it. Avoids MANAGE_EXTERNAL_STORAGE
     *  (removed for Play compliance) — uses a documents URI + clipboard. */
    private fun openOrCopyDataFolder(context: android.content.Context) {
        val path = Main.systemDirPosix()
            ?: context.getExternalFilesDir(null)?.absolutePath ?: ""
        val opened = runCatching {
            val docId = "primary:Android/data/${context.packageName}/files"
            val uri = android.provider.DocumentsContract.buildDocumentUri(
                "com.android.externalstorage.documents", docId)
            val intent = android.content.Intent(android.content.Intent.ACTION_VIEW).apply {
                setDataAndType(uri, android.provider.DocumentsContract.Document.MIME_TYPE_DIR)
                addFlags(
                    android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION or
                        android.content.Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                )
            }
            context.startActivity(intent)
            true
        }.getOrDefault(false)
        if (!opened) {
            runCatching {
                val cb = context.getSystemService(android.content.Context.CLIPBOARD_SERVICE)
                    as android.content.ClipboardManager
                cb.setPrimaryClip(android.content.ClipData.newPlainText("ARMSX2 data folder", path))
            }
            android.widget.Toast.makeText(
                context,
                "No file manager could open it here. Path copied:\n$path",
                android.widget.Toast.LENGTH_LONG,
            ).show()
        }
    }

    @Composable
    private fun NavGlyph(kind: NavKind, active: Boolean) {
        val color = Color.White.copy(alpha = if (active) 1f else 0.78f)
        val cogPath = remember { PathParser().parsePathString(LibIcons.SETTINGS_COG).toPath() }
        Canvas(Modifier.size(36.dp)) {
            val min = size.minDimension
            val stroke = Stroke(width = min * 0.075f, cap = StrokeCap.Round)
            when (kind) {
                NavKind.Library -> {
                    val cell = min * 0.24f
                    val gap = min * 0.16f
                    val total = cell * 2f + gap
                    val left = (size.width - total) / 2f
                    val top = (size.height - total) / 2f
                    repeat(2) { row ->
                        repeat(2) { col ->
                            drawRoundRect(
                                color = color,
                                topLeft = Offset(left + col * (cell + gap), top + row * (cell + gap)),
                                size = Size(cell, cell),
                                cornerRadius = CornerRadius(cell * 0.18f, cell * 0.18f),
                                style = stroke,
                            )
                        }
                    }
                }
                NavKind.Settings -> {
                    // vivi's cog outline, scaled from its 95x95 SVG viewBox to the glyph
                    // box. Stroke width 5 is in path space, so it scales down with the path
                    // to the SVG's original 5-on-95 weight and reads at rail density.
                    val unit = min / 95f
                    scale(unit, unit, pivot = Offset.Zero) {
                        drawPath(
                            cogPath,
                            color = color,
                            style = Stroke(
                                width = 5f,
                                cap = StrokeCap.Round,
                                join = StrokeJoin.Round,
                            ),
                        )
                    }
                }
            }
        }
    }

    @Composable
    private fun SectionHeader(text: String, modifier: Modifier = Modifier) {
        Text(
            text,
            color = Color.White,
            fontSize = 24.sp,
            fontWeight = FontWeight.Bold,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            modifier = modifier,
        )
    }

    @Composable
    private fun GameShelf(
        games: List<GameInfo>,
        label: String?,
        rowId: Int,
        listItemIndex: Int,
        coverWidth: Dp,
        modifier: Modifier = Modifier,
        nowPlaying: Boolean = false,
    ) {
        val rowState = rememberLazyListState()
        val selectedUri = controllerSelectedUri.value
        val density = LocalDensity.current

        LaunchedEffect(selectedUri, games) {
            val index = controllerSelectedColumnIndex(rowId) ?: return@LaunchedEffect
            val info = rowState.layoutInfo
            val itemInfo = info.visibleItemsInfo.firstOrNull { it.index == index }
            if (itemInfo == null) {
                // The cover isn't rendered at all — bring it to the leading edge.
                rowState.animateScrollToItem(index)
                return@LaunchedEffect
            }
            // Visible but possibly clipped at an edge. Nudge by exactly the
            // overflow (plus a small margin for the selection glow) so the whole
            // cover lands inside the viewport. The old code only scrolled when the
            // cover was fully past the last visible item, so the rightmost covers
            // — which sit half-visible as the trailing item — never slid in.
            val margin = with(density) { 18.dp.toPx() }
            val itemStart = itemInfo.offset.toFloat()
            val itemEnd = (itemInfo.offset + itemInfo.size).toFloat()
            val delta = when {
                itemStart - margin < info.viewportStartOffset ->
                    itemStart - margin - info.viewportStartOffset
                itemEnd + margin > info.viewportEndOffset ->
                    itemEnd + margin - info.viewportEndOffset
                else -> 0f
            }
            if (delta != 0f) rowState.animateScrollBy(delta)
        }

        Box(modifier) {
            ShelfGlass(
                label = label,
                nowPlaying = nowPlaying,
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .fillMaxWidth()
                    .height(76.dp),
            )
            LazyRow(
                state = rowState,
                modifier = Modifier.fillMaxSize(),
                contentPadding = PaddingValues(horizontal = 26.dp),
                horizontalArrangement = Arrangement.spacedBy(28.dp),
                verticalAlignment = Alignment.Bottom,
            ) {
                lazyItemsIndexed(games, key = { index, game -> "shelf_${listItemIndex}_${index}_${game.uri}" }) { columnIndex, game ->
                    ShelfGameCard(
                        game = game,
                        rowId = rowId,
                        columnIndex = columnIndex,
                        width = coverWidth,
                        modifier = Modifier.padding(bottom = 22.dp),
                    )
                }
            }
        }
    }

    @Composable
    private fun ShelfGlass(label: String?, nowPlaying: Boolean = false, modifier: Modifier = Modifier) {
        if (nowPlaying) {
            // vivi's "Now Playing" shelf — ONLY the Recently Played row. Her 2850x159
            // SVG shape, but filled solid (dark front lip + brighter top surface) so it
            // reads as a clean glossy ledge instead of a faint translucent wash.
            val lower = remember { PathParser().parsePathString(LibIcons.SHELF_LOWER).toPath() }
            val upper = remember { PathParser().parsePathString(LibIcons.SHELF_UPPER).toPath() }
            Canvas(modifier) {
                if (size.width <= 0f || size.height <= 0f) return@Canvas
                scale(size.width / 2850f, size.height / 159f, pivot = Offset.Zero) {
                    drawPath(lower, color = Color(0xE0142B4E)) // front face — dark navy
                    drawPath(upper, color = Color(0xC84069AE)) // top surface — brighter blue
                }
            }
        } else {
            // Original frosted-glass shelf (cyan), kept for every Library row.
            Canvas(modifier) {
                val w = size.width
                val h = size.height
                val stroke = Stroke(width = 1.dp.toPx())
                val top = Path().apply {
                    moveTo(w * 0.10f, h * 0.04f)
                    lineTo(w * 0.92f, h * 0.04f)
                    lineTo(w * 0.995f, h * 0.46f)
                    lineTo(w * 0.005f, h * 0.46f)
                    close()
                }
                val face = Path().apply {
                    moveTo(w * 0.005f, h * 0.46f)
                    lineTo(w * 0.995f, h * 0.46f)
                    lineTo(w * 0.975f, h * 0.82f)
                    lineTo(w * 0.025f, h * 0.82f)
                    close()
                }
                drawPath(
                    top,
                    brush = Brush.linearGradient(
                        colors = listOf(Color(0x88D8F5FF), Color(0x442A76B4), Color(0x77E9FBFF)),
                    ),
                )
                drawPath(
                    face,
                    brush = Brush.verticalGradient(
                        colors = listOf(Color(0x55C9F4FF), Color(0x303E607D), Color(0x1AFFFFFF)),
                    ),
                )
                drawPath(top, Color.White.copy(alpha = 0.18f), style = stroke)
                drawPath(face, Color.White.copy(alpha = 0.14f), style = stroke)
            }
        }
        if (label != null) {
            Text(
                label,
                color = Color.White.copy(alpha = 0.35f),
                fontSize = 18.sp,
                modifier = modifier.padding(top = 42.dp),
                textAlign = TextAlign.Center,
            )
        }
    }

    @OptIn(ExperimentalFoundationApi::class)
    /** A single full-width name row for the compact LIST view (no cover art) —
     *  built for fast finding on small screens. Shares the grid controller-nav
     *  model (perRow == 1, so each list row is its own controller row at column 0)
     *  and the same tap-to-launch / long-press-for-settings gestures as a card. */
    @Composable
    private fun ListGameRow(game: GameInfo, rowId: Int) {
        var rowFocused by remember { mutableStateOf(false) }
        val glowBlue = Color(0xFF3DA5FF)
        val selectedUri = controllerSelectedUri.value
        val gridFocused = controllerZone.value == Zone.GRID
        val highlighted = rowFocused ||
            (gridFocused && selectedUri == game.uri.toString() && controllerCellSelected(rowId, 0))
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(46.dp)
                .onFocusChanged {
                    rowFocused = it.isFocused
                    if (it.isFocused) selectControllerCell(rowId, 0, game.uri.toString())
                }
                .clip(RoundedCornerShape(6.dp))
                .background(if (highlighted) glowBlue.copy(alpha = 0.22f) else Color.White.copy(alpha = 0.04f))
                .then(if (highlighted) Modifier.border(2.dp, glowBlue, RoundedCornerShape(6.dp)) else Modifier)
                .combinedClickable(
                    onClick = { launchGame(game) },
                    onLongClick = { InGameOverlay.openGameSettings(game) },
                )
                .padding(horizontal = 14.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            game.regionFlag?.let { flag ->
                Text(flag, fontSize = 16.sp, maxLines = 1, modifier = Modifier.padding(end = 10.dp))
            }
            Text(
                game.title,
                color = Color.White.copy(alpha = if (highlighted) 1f else 0.88f),
                fontFamily = TitleFont,
                fontSize = 18.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier.weight(1f),
            )
            game.versionTag?.let { tag ->
                Text(
                    tag,
                    color = Color.White.copy(alpha = 0.4f),
                    fontFamily = TitleFont,
                    fontSize = 12.sp,
                    maxLines = 1,
                    modifier = Modifier.padding(start = 8.dp),
                )
            }
        }
    }

    @OptIn(ExperimentalFoundationApi::class)
    @Composable
    private fun ShelfGameCard(
        game: GameInfo,
        rowId: Int,
        columnIndex: Int,
        width: Dp,
        modifier: Modifier = Modifier,
    ) {
        var cardFocused by remember { mutableStateOf(false) }
        val glowBlue = Color(0xFF3DA5FF)
        // Read the selection STATE (controllerSelectedUri) here so the card
        // recomposes when the controller moves the selection. The row/col
        // indices behind controllerCellSelected are plain vars, so without a
        // state read the glow stays frozen even though the model updates.
        val selectedUri = controllerSelectedUri.value
        val gridFocused = controllerZone.value == Zone.GRID
        val highlighted = cardFocused ||
            (gridFocused && selectedUri == game.uri.toString() &&
                controllerCellSelected(rowId, columnIndex))
        Column(
            modifier = modifier
                .width(width)
                .onFocusChanged {
                    cardFocused = it.isFocused
                    if (it.isFocused) selectControllerCell(rowId, columnIndex, game.uri.toString())
                }
                .combinedClickable(
                    onClick = { launchGame(game) },
                    onLongClick = { InGameOverlay.openGameSettings(game) },
                ),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            CoverArt(
                game = game,
                modifier = Modifier
                    .fillMaxWidth()
                    .aspectRatio(0.7f)
                    // Controller selection highlight: blue glow + outline on
                    // the cover when this card has D-pad focus.
                    .then(
                        if (highlighted)
                            Modifier.shadow(14.dp, RoundedCornerShape(6.dp), ambientColor = glowBlue, spotColor = glowBlue)
                        else Modifier
                    )
                    .clip(RoundedCornerShape(5.dp))
                    .then(
                        if (highlighted)
                            Modifier.border(3.dp, glowBlue, RoundedCornerShape(5.dp))
                        else Modifier
                    ),
            )
            Box(
                Modifier
                    .fillMaxWidth()
                    .height(34.dp)
                    .clip(RoundedCornerShape(3.dp)),
            ) {
                CoverArt(
                    game = game,
                    modifier = Modifier
                        .fillMaxWidth()
                        .aspectRatio(0.7f)
                        .graphicsLayer {
                            scaleY = -1f
                            alpha = 0.22f
                        },
                )
                Box(
                    Modifier
                        .matchParentSize()
                        .background(
                            Brush.verticalGradient(
                                listOf(
                                    Color(0x0000172D),
                                    Color(0xAA00172D),
                                ),
                            ),
                        ),
                )
                // Region flag merged INTO the reflection (vivi's request): it sits
                // over the mirrored cover instead of as its own row underneath, so
                // the vertical space is reclaimed and the flag reads as part of the
                // cover. Users still tell regional versions apart at a glance.
                game.regionFlag?.let { flag ->
                    Text(
                        flag,
                        fontSize = 15.sp,
                        maxLines = 1,
                        textAlign = TextAlign.Center,
                        modifier = Modifier.align(Alignment.Center),
                    )
                }
            }
            // Optional game title under the cover, toggled from the left rail.
            // Fixed-height title area: 1-line vs 2-line titles (and the optional
            // version tag) must NOT change the card height, or the bottom-aligned
            // shelf row renders the covers at uneven heights. Reserve a constant
            // block (matches the shelf's titlesExtra allowance) and top-align it.
            if (LibraryTitles.show.value) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(44.dp),
                    horizontalAlignment = Alignment.CenterHorizontally,
                ) {
                    Text(
                        game.title,
                        color = Color.White.copy(alpha = 0.9f),
                        fontFamily = TitleFont,
                        fontSize = 12.sp,
                        lineHeight = 13.sp,
                        maxLines = 2,
                        textAlign = TextAlign.Center,
                        overflow = TextOverflow.Ellipsis,
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(start = 2.dp, end = 2.dp, top = 3.dp),
                    )
                    // Version/edition tag (disc version from the filename, else serial)
                    // so two copies of the same game are distinguishable.
                    game.versionTag?.let { tag ->
                        Text(
                            tag,
                            color = Color.White.copy(alpha = 0.45f),
                            fontFamily = TitleFont,
                            fontSize = 10.sp,
                            lineHeight = 11.sp,
                            maxLines = 1,
                            textAlign = TextAlign.Center,
                            overflow = TextOverflow.Ellipsis,
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(start = 2.dp, end = 2.dp),
                        )
                    }
                }
            }
        }
    }

    private fun shelfLabelFor(row: List<GameInfo>): String {
        fun labelFor(game: GameInfo): String =
            game.title.firstOrNull { it.isLetterOrDigit() }?.uppercaseChar()?.toString() ?: "#"
        if (row.isEmpty()) return ""
        val first = labelFor(row.first())
        val last = labelFor(row.last())
        return if (first == last) first else "$first - $last"
    }

    private fun launchGame(game: GameInfo) {
        controllerSelectedUri.value = game.uri.toString()
        WindowImpl.showLibrary.value = false
        markRecentlyPlayed(game)
        // ELF/homebrew from the library: emucore's AutoDetectSource keys off a real
        // ".elf" file path (s_elf_override / NoDisc). A file:// ELF boots in place
        // (handled by the normal path below); a content:// ELF (SAF-scanned) is
        // copied to a real .elf file first, mirroring the toolbar ELF picker. The
        // ORIGINAL GameInfo is kept so settingsKey (filename stem) and covers stay
        // stable (#253).
        if (game.extension == "ELF" && game.uri.scheme != "file") {
            val ctx = Main.instance?.applicationContext
            if (ctx != null) {
                val base = game.uri.lastPathSegment?.substringAfterLast('/')?.substringAfterLast(':')
                    ?.takeIf { it.isNotEmpty() } ?: "${game.title}.elf"
                val outName = if (base.endsWith(".elf", ignoreCase = true)) base else "$base.elf"
                val outFile = File(File(Main.assetCopyRoot(ctx), "elf").apply { mkdirs() }, outName)
                val copied = runCatching {
                    ctx.contentResolver.openInputStream(game.uri)?.use { ins ->
                        outFile.outputStream().use { outs -> ins.copyTo(outs) }
                    } != null
                }.getOrDefault(false)
                if (copied && outFile.length() > 0L) {
                    Main.launchGame(outFile.absolutePath, game)
                    return
                }
                Toast.makeText(ctx, I18n.get("games.toast.couldntLoadElf"), Toast.LENGTH_SHORT).show()
                return
            }
        }
        // Raw-mode games (all-files build) carry a file:// URI — hand the core the
        // bare /storage path so CDVD opens it directly, not via the SAF FD bridge.
        val launchArg = if (game.uri.scheme == "file") (game.uri.path ?: game.uri.toString())
            else game.uri.toString()
        Main.launchGame(launchArg, game)
    }

    private fun updateControllerLayout(controllerRowsForUi: List<ControllerGameRow>) {
        controllerRows = controllerRowsForUi.filter { it.games.isNotEmpty() }
        if (controllerRows.isEmpty()) {
            controllerRowIndex = 0
            controllerColumnIndex = 0
            controllerSelectedUri.value = null
            return
        }

        val selected = controllerSelectedUri.value
        val rowIndex = controllerRowIndex.coerceIn(0, controllerRows.lastIndex)
        val columnIndex = controllerColumnIndex.coerceIn(0, controllerRows[rowIndex].games.lastIndex)
        if (selected != null && controllerRows[rowIndex].games[columnIndex].uri.toString() == selected) {
            selectControllerPosition(controllerRows, rowIndex, columnIndex)
            return
        }

        val found = selected?.let { findControllerPosition(it) }
        if (found != null)
            selectControllerPosition(controllerRows, found.rowIndex, found.columnIndex)
        else
            selectControllerPosition(controllerRows, 0, 0)
    }

    private fun controllerSelectedPosition(rows: List<ControllerGameRow> = controllerRows): ControllerGamePosition {
        val rowIndex = controllerRowIndex.coerceIn(0, rows.lastIndex)
        val columnIndex = controllerColumnIndex.coerceIn(0, rows[rowIndex].games.lastIndex)
        return ControllerGamePosition(rowIndex, columnIndex, rows[rowIndex].games[columnIndex])
    }

    private fun selectControllerPosition(rows: List<ControllerGameRow>, rowIndex: Int, columnIndex: Int) {
        controllerRowIndex = rowIndex.coerceIn(0, rows.lastIndex)
        controllerColumnIndex = columnIndex.coerceIn(0, rows[controllerRowIndex].games.lastIndex)
        controllerSelectedUri.value = rows[controllerRowIndex].games[controllerColumnIndex].uri.toString()
    }

    private fun findControllerPosition(uri: String): ControllerGamePosition? {
        controllerRows.forEachIndexed { rowIndex, row ->
            val columnIndex = row.games.indexOfFirst { it.uri.toString() == uri }
            if (columnIndex >= 0)
                return ControllerGamePosition(rowIndex, columnIndex, row.games[columnIndex])
        }
        return null
    }

    private fun selectControllerCell(rowId: Int, columnIndex: Int, uri: String) {
        val rowIndex = controllerRows.indexOfFirst { it.rowId == rowId }
        if (rowIndex >= 0)
            selectControllerPosition(controllerRows, rowIndex, columnIndex)
        else
            controllerSelectedUri.value = uri
    }

    private fun controllerCellSelected(rowId: Int, columnIndex: Int): Boolean {
        if (controllerRows.isEmpty()) return false
        val selected = controllerSelectedPosition()
        return controllerRows[selected.rowIndex].rowId == rowId &&
            selected.columnIndex == columnIndex
    }

    private fun controllerSelectedListItemIndex(): Int? {
        if (controllerRows.isEmpty()) return null
        return controllerRows[controllerSelectedPosition().rowIndex].listItemIndex
    }

    private fun controllerSelectedColumnIndex(rowId: Int): Int? {
        if (controllerRows.isEmpty()) return null
        val selected = controllerSelectedPosition()
        return if (controllerRows[selected.rowIndex].rowId == rowId)
            selected.columnIndex
        else
            null
    }

    private fun loadCustomBackground(context: Context) {
        val saved = Main.prefs.getString(KEY_LIBRARY_BACKGROUND, null)
            ?.takeIf { File(it).exists() }
        customBackgroundPath.value = saved
    }

    private fun importCustomBackground(context: Context, uri: Uri) {
        val name = DocumentFile.fromSingleUri(context, uri)?.name.orEmpty()
        // gif/webp/png(APNG) allowed so animated backgrounds keep their real
        // extension; Coil (with the GIF decoder) animates them. Others fall to jpg.
        val ext = name.substringAfterLast('.', missingDelimiterValue = "jpg")
            .lowercase()
            .takeIf { it in setOf("jpg", "jpeg", "png", "webp", "gif") }
            ?: "jpg"
        val outDir = File(Main.assetCopyRoot(context), "backgrounds").apply { mkdirs() }
        val outFile = File(outDir, "library_background.$ext")
        val ok = runCatching {
            context.contentResolver.openInputStream(uri)?.use { input ->
                outFile.outputStream().use { output -> input.copyTo(output) }
            } != null && outFile.length() > 0L
        }.getOrDefault(false)
        if (ok) {
            Main.prefs.edit().putString(KEY_LIBRARY_BACKGROUND, outFile.absolutePath).apply()
            customBackgroundPath.value = outFile.absolutePath
            Toast.makeText(context, I18n.get("games.toast.backgroundImported"), Toast.LENGTH_SHORT).show()
        } else {
            Toast.makeText(context, I18n.get("games.toast.backgroundImportFailed"), Toast.LENGTH_LONG).show()
        }
    }

    private fun resetCustomBackground(context: Context) {
        customBackgroundPath.value?.let { runCatching { File(it).delete() } }
        customBackgroundPath.value = null
        Main.prefs.edit().remove(KEY_LIBRARY_BACKGROUND).apply()
        Toast.makeText(context, I18n.get("games.toast.backgroundReset"), Toast.LENGTH_SHORT).show()
    }

    @Composable
    private fun EmptyMessage(title: String, body: String) {
        Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.Center) {
            Text(title, color = Color.White, fontSize = 18.sp,
                modifier = Modifier.align(Alignment.CenterHorizontally))
            Spacer(Modifier.height(8.dp))
            Text(body, color = Color.LightGray, fontSize = 14.sp,
                modifier = Modifier.align(Alignment.CenterHorizontally))
        }
    }

    @Composable
    private fun ScanningSpinner() {
        Row(Modifier.fillMaxSize(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.Center) {
            CircularProgressIndicator(
                modifier = Modifier.size(24.dp),
                strokeWidth = 2.dp,
                color = Colors.pasx2_blue,
            )
            Spacer(Modifier.width(12.dp))
            Text(str("games.scanningRoms"), color = Color.White)
        }
    }

    /** First grid slot — a refresh button styled like a game card.
     *  Caller passes Modifier.weight(1f) so this card and the BIOS card
     *  split the slot evenly. The icon Box uses weight(1f) too so the
     *  icon centers in whatever vertical space is left after the label. */
    @Composable
    private fun RefreshCard(
        modifier: Modifier = Modifier,
        isScanning: Boolean,
        onRefresh: () -> Unit,
    ) {
        Column(
            modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(8.dp))
                // Already partial-alpha at 0.20, leave as-is; matches the
                // see-through aesthetic of the regular GameCard chrome.
                .background(Colors.pasx2_blue.copy(alpha = 0.20f))
                .border(2.dp, Colors.pasx2_blue.copy(alpha = 0.3f), RoundedCornerShape(8.dp))
                .clickable(enabled = !isScanning, onClick = onRefresh),
        ) {
            Box(
                Modifier
                    .fillMaxWidth()
                    .weight(1f),
                contentAlignment = Alignment.Center,
            ) {
                if (isScanning) {
                    CircularProgressIndicator(
                        modifier = Modifier.size(36.dp),
                        strokeWidth = 3.dp,
                        color = Color.White,
                    )
                } else {
                    Text("⟳", color = Color.White, fontSize = 56.sp)
                }
            }
            Column(Modifier.padding(8.dp)) {
                Text(
                    if (isScanning) str("games.card.scanning") else str("games.card.refresh"),
                    color = Color.White,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Bold,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                Spacer(Modifier.height(2.dp))
                Text(
                    str("games.card.rescanRomsFolder"),
                    color = Color.LightGray,
                    fontSize = 11.sp,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }
        }
    }

    /** Stacked under RefreshCard in the first grid slot — boots the
     *  PS2 BIOS without a disc, leaving the library/toolbar visible.
     *  The caller passes Modifier.weight(1f) so this card stretches to
     *  match the height of adjacent GameCards. The icon is centered in
     *  the flexible region; the label sits at the bottom. */
    @Composable
    private fun BiosCard(modifier: Modifier = Modifier, onLaunch: () -> Unit) {
        Column(
            modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(8.dp))
                .background(Colors.pasx2_blue.copy(alpha = 0.20f))
                .border(2.dp, Colors.pasx2_blue.copy(alpha = 0.3f), RoundedCornerShape(8.dp))
                .clickable(onClick = onLaunch),
        ) {
            Box(
                Modifier
                    .fillMaxWidth()
                    .weight(1f),
                contentAlignment = Alignment.Center,
            ) {
                Text("▶", color = Color.White, fontSize = 56.sp)
            }
            Column(Modifier.padding(8.dp)) {
                Text(
                    str("games.card.startBios"),
                    color = Color.White,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Bold,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                Spacer(Modifier.height(2.dp))
                Text(
                    str("games.card.bootWithoutDisc"),
                    color = Color.LightGray,
                    fontSize = 11.sp,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }
        }
    }

    @Composable
    private fun UtilityCard(
        modifier: Modifier = Modifier,
        title: String,
        subtitle: String,
        onClick: () -> Unit,
    ) {
        Column(
            modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(8.dp))
                .background(Color(0xFF272525).copy(alpha = 0.35f))
                .border(1.dp, Colors.pasx2_blue.copy(alpha = 0.25f), RoundedCornerShape(8.dp))
                .clickable(onClick = onClick)
                .padding(10.dp),
            verticalArrangement = Arrangement.Center,
        ) {
            Text(
                title,
                color = Color.White,
                fontSize = 13.sp,
                fontWeight = FontWeight.Bold,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Spacer(Modifier.height(3.dp))
            Text(
                subtitle,
                color = Color.LightGray,
                fontSize = 10.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
    }

    @OptIn(ExperimentalFoundationApi::class)
    @Composable
    private fun GameCard(game: GameInfo) {
        val selected = controllerSelectedUri.value == game.uri.toString()
        val glowBlue = Color(0xFF3DA5FF)
        Column(
            Modifier
                .fillMaxWidth()
                .then(
                    if (selected)
                        Modifier.shadow(14.dp, RoundedCornerShape(8.dp), ambientColor = glowBlue, spotColor = glowBlue)
                    else Modifier
                )
                .clip(RoundedCornerShape(8.dp))
                // Card chrome (background + border) is partial-alpha so the
                // live game surface shows through when the library is
                // overlaid mid-play. The cover image painted into the inner
                // Box and the title/serial Text below stay fully opaque
                // because they're separate paint layers — alpha here only
                // affects the chrome, not the children.
                .background(Color(0xFF272525).copy(alpha = 0.3f))
                .border(
                    if (selected) 2.dp else 1.dp,
                    if (selected) glowBlue else Color(0xFF3A3A3A).copy(alpha = 0.3f),
                    RoundedCornerShape(8.dp),
                )
                .combinedClickable(
                    onClick = {
                        // Clear the library overlay (set by LoadGameButton while
                        // a game was running) so the surface for the new game
                        // comes up uncovered. No-op when starting from idle.
                        // Pass GameInfo so the in-game overlay has cover art /
                        // extension badge / pre-resolved compat stars ready.
                        launchGame(game)
                    },
                    onLongClick = { InGameOverlay.openGameSettings(game) },
                ),
        ) {
            CoverArt(
                game = game,
                modifier = Modifier
                    .fillMaxWidth()
                    .aspectRatio(0.7f),
            )
            Column(Modifier.padding(8.dp)) {
                Text(
                    game.title,
                    color = Color.White,
                    fontSize = 13.sp,
                    fontWeight = FontWeight.Bold,
                    // Always reserve 2 lines so 1-line titles don't make
                    // their cards shorter than 2-line ones — keeps the
                    // grid rows uniform.
                    minLines = 2,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                )
                Spacer(Modifier.height(2.dp))
                Text(
                    game.serial?.let { s -> game.region?.let { "$s · $it" } ?: s } ?: str("games.unknownSerial"),
                    color = if (game.serial != null) Color(0xFFAACCFF) else Color(0xFF6F6F6F),
                    fontSize = 10.sp,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                Spacer(Modifier.height(4.dp))
                Row(
                    Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    CompatibilityStars(game.compatibility)
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        if (game.extension.isNotEmpty()) ExtensionBadge(game.extension)
                        // Hardcore mode is a global RetroAchievements flag —
                        // when it's on, ANY game launched from here will
                        // apply, so render the HC badge on every card. The
                        // overlay's polling loop owns the flag.
                        if (InGameOverlay.hardcoreOn.value) {
                            Spacer(Modifier.width(4.dp))
                            HardcoreBadge()
                        }
                    }
                }
            }
        }
    }

    @Composable
    private fun CoverArt(game: GameInfo, modifier: Modifier = Modifier) {
        val context = LocalContext.current
        Box(
            modifier
                .background(Color(0xFF1B1A1A).copy(alpha = 0.3f)),
            contentAlignment = Alignment.Center,
        ) {
            // A user-set custom cover wins over the online repo — and is the only
            // cover source for serial-less games (homebrew, ELF ports). Resolved
            // SYNCHRONOUSLY from the preloaded index (customCoverMap), so cover
            // tiles don't each do their own dir listing during a scroll.
            val custom = remember(game.uri, customCoverMap.value) {
                CustomCovers.matchIn(customCoverMap.value, game)
            }
            val coverUrl = game.coverUrl
            if (custom != null) {
                val scale = when (game.platform) {
                    GamePlatform.PS2 -> ContentScale.Crop
                    GamePlatform.PS1 -> ContentScale.Fit
                }
                AsyncImage(
                    model = ImageRequest.Builder(context).data(custom).crossfade(true).build(),
                    contentDescription = "${game.title} cover",
                    contentScale = scale,
                    alignment = Alignment.Center,
                    modifier = Modifier.fillMaxSize(),
                )
            } else if (coverUrl != null) {
                val use3d = CoverArtStyle.use3d.value // re-key cache on style change
                val coverFile = remember(game.serial, game.platform, use3d) { coverFileFor(context, game) }
                // Resolve the local cover entirely off the main thread — the
                // exists()/length() stat and any download previously ran during
                // composition, hitching the scroll as tiles streamed in.
                val localCoverReady = remember(coverFile?.absolutePath) { mutableStateOf(false) }
                LaunchedEffect(coverUrl, coverFile?.absolutePath) {
                    val file = coverFile ?: return@LaunchedEffect
                    localCoverReady.value = withContext(Dispatchers.IO) {
                        if (file.exists() && file.length() > 0L) true
                        else mirrorCoverToFile(coverUrl, file)
                    }
                }
                // SubcomposeAsyncImage so we can render a real fallback
                // composable when the cover URL 404s — happens for any
                // game the xlenore covers repo doesn't have (obscure
                // releases, regional dumps whose serial isn't covered).
                //
                // Per-platform contentScale: PS2 boxart is taller than
                // wide (matches the cell's 0.7 aspect ratio cleanly), so
                // ContentScale.Crop fills the cell with no margin. PS1
                // jewel-case covers are squarer/wider — cropping would
                // chop the sides; ContentScale.Fit + Center letterboxes
                // the cover inside the taller cell so the full art is
                // visible. Cell aspect stays uniform so the grid layout
                // doesn't shift between platforms.
                val scale = when (game.platform) {
                    GamePlatform.PS2 -> ContentScale.Crop
                    GamePlatform.PS1 -> ContentScale.Fit
                }
                val coverModel: Any = if (localCoverReady.value && coverFile != null) coverFile else coverUrl
                // AsyncImage, NOT SubcomposeAsyncImage: the latter sub-composes per
                // tile and, paired with the animated CircularProgressIndicator
                // loading slot, made the library scroll janky once a lot of games
                // were on screen (continuous invalidation per loading tile). AsyncImage
                // has no per-item subcomposition and downsamples to the tile size; the
                // Box's dark background shows during load (no spinner), and on a 404 we
                // flip to the no-cover fallback once via onError.
                val coverFailed = remember(coverModel) { mutableStateOf(false) }
                if (coverFailed.value) {
                    NoCoverTile(missingSerial = false)
                } else {
                    AsyncImage(
                        model = ImageRequest.Builder(context)
                            .data(coverModel)
                            .crossfade(true)
                            .build(),
                        contentDescription = "${game.title} cover",
                        contentScale = scale,
                        alignment = Alignment.Center,
                        modifier = Modifier.fillMaxSize(),
                        onError = { coverFailed.value = true },
                    )
                }
            } else {
                NoCoverTile(missingSerial = true)
            }
        }
    }

    // Covers dir. Delegates to CustomCovers.coversRoot so remote AND custom covers
    // resolve to the SAME cached root — Main.assetCopyRoot can flip between the
    // system dir and the app-private fallback on a transient write-probe, and a
    // per-call resolve made custom covers land in a different dir than they were
    // saved to. One shared cache keeps them consistent (and off the per-tile path).
    private fun coversDir(context: Context): File = CustomCovers.coversRoot(context)

    private fun coverFileFor(context: Context, game: GameInfo): File? {
        val serial = game.serial ?: return null
        val coversDir = coversDir(context)
        // Cache each style separately so toggling 2D/3D doesn't reuse the
        // wrong cached image.
        return if (CoverArtStyle.use3d.value)
            File(coversDir, "${serial}_3d.png")
        else
            File(coversDir, "$serial.jpg")
    }

    private suspend fun mirrorCoverToFile(url: String, target: File): Boolean = withContext(Dispatchers.IO) {
        if (target.exists() && target.length() > 0L)
            return@withContext true

        val parent = target.parentFile ?: return@withContext false
        if (!parent.exists() && !parent.mkdirs())
            return@withContext false

        val tmp = File(parent, ".${target.name}.${System.nanoTime()}.tmp")
        var conn: HttpURLConnection? = null
        try {
            conn = (URL(url).openConnection() as HttpURLConnection).apply {
                connectTimeout = 10_000
                readTimeout = 15_000
                instanceFollowRedirects = true
                setRequestProperty("User-Agent", "ARMSX2 Android")
            }
            if (conn.responseCode !in 200..299)
                return@withContext false

            conn.inputStream.use { input ->
                tmp.outputStream().use { output ->
                    input.copyTo(output)
                }
            }
            if (tmp.length() <= 0L)
                return@withContext false

            if (target.exists() && !target.delete())
                return@withContext false
            if (!tmp.renameTo(target)) {
                tmp.copyTo(target, overwrite = true)
                tmp.delete()
            }
            target.exists() && target.length() > 0L
        } catch (_: Exception) {
            false
        } finally {
            tmp.delete()
            conn?.disconnect()
        }
    }

    private fun markRecentlyPlayed(game: GameInfo) {
        val uri = game.uri.toString()
        recentUris.remove(uri)
        recentUris.add(0, uri)
        while (recentUris.size > 10) recentUris.removeAt(recentUris.lastIndex)
        saveRecents()
    }

    private fun loadRecents() {
        val raw = Main.prefs.getString("recentGameUris", null) ?: return
        try {
            val arr = JSONArray(raw)
            recentUris.clear()
            for (i in 0 until arr.length()) {
                val uri = arr.optString(i)
                if (uri.isNotEmpty() && uri !in recentUris) recentUris.add(uri)
            }
        } catch (_: Exception) {
            recentUris.clear()
        }
    }

    private fun saveRecents() {
        val arr = JSONArray()
        recentUris.forEach { arr.put(it) }
        Main.prefs.edit().putString("recentGameUris", arr.toString()).apply()
    }

    /** Small PS2-blue rounded chip showing the container format (ISO /
     *  CHD / BIN / etc.). Sits to the right of the compatibility stars. */
    @Composable
    private fun ExtensionBadge(ext: String) {
        Box(
            Modifier
                .clip(RoundedCornerShape(4.dp))
                .background(Colors.pasx2_blue)
                .padding(horizontal = 5.dp, vertical = 1.dp),
        ) {
            Text(
                ext,
                color = Color.White,
                fontSize = 9.sp,
                fontWeight = FontWeight.Bold,
                maxLines = 1,
            )
        }
    }

    /** RetroAchievements Hardcore badge. Shown on library cards while the
     *  global hardcore flag is set (so the user knows whichever game they
     *  launch will run in HC). Same shape as ExtensionBadge for visual
     *  consistency. */
    @Composable
    private fun HardcoreBadge() {
        Box(
            Modifier
                .clip(RoundedCornerShape(4.dp))
                .background(Color(0xFFB22222))
                .padding(horizontal = 5.dp, vertical = 1.dp),
        ) {
            Text(
                "HC",
                color = Color.White,
                fontSize = 9.sp,
                fontWeight = FontWeight.Bold,
                maxLines = 1,
            )
        }
    }

    /** Subtle spinner shown while the network fetch for a cover is in
     *  flight. Same dim background as the empty cover tile so the tile
     *  doesn't flash bright while loading. */
    @Composable
    private fun CoverLoadingTile() {
        Box(
            Modifier.fillMaxSize().background(Color(0xFF1B1A1A).copy(alpha = 0.3f)),
            contentAlignment = Alignment.Center,
        ) {
            CircularProgressIndicator(
                modifier = Modifier.size(20.dp),
                strokeWidth = 2.dp,
                color = Color(0xFF555555),
            )
        }
    }

    /** Fallback cover tile rendered when:
     *    - missingSerial = true  (we couldn't extract a serial at all
     *                             → no URL to even try) → "?" in red-ish tint
     *    - missingSerial = false (URL was tried and 404'd / network err)
     *                            → disc icon + "No cover" subtitle
     *  In both cases the title + serial are still visible below the tile.
     */
    @Composable
    private fun NoCoverTile(missingSerial: Boolean) {
        Box(
            Modifier.fillMaxSize().background(Color(0xFF1B1A1A).copy(alpha = 0.3f)),
            contentAlignment = Alignment.Center,
        ) {
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                if (missingSerial) {
                    Text("?", color = Color(0xFF8A4A4A), fontSize = 56.sp)
                    Text(str("games.noSerial"), color = Color(0xFF8A4A4A), fontSize = 10.sp)
                } else {
                    Text("📀", color = Color(0xFF3F3F3F), fontSize = 56.sp)
                    Text(str("games.noCover"), color = Color(0xFF6F6F6F), fontSize = 10.sp)
                }
            }
        }
    }

    @Composable
    private fun CompatibilityStars(filled: Int) {
        Row {
            repeat(5) { i ->
                val on = i < filled
                Text(
                    if (on) "★" else "☆",
                    color = if (on) Color(0xFFFFD33A) else Color(0xFF555555),
                    fontSize = 12.sp,
                )
            }
        }
    }

    /**
     * Walk every configured ROM folder via DocumentFile, probe each disc
     * image's SYSTEM.CNF for its real serial + platform when possible,
     * and persist merged results to prefs. Falls back to filename-based
     * serial extraction for compressed formats (CHD/CSO/GZ) or non-ISO9660
     * raw images.
     *
     * De-duplication: same URI in two configured folders (rare — would
     * require nested SAF mounts) is merged by URI string. Title sort is
     * case-insensitive across the union.
     */
    private fun scanRoms(context: Context, romsUriStrings: List<String>, romsKey: String) {
        scanning.value = true
        scanError.value = null
        lastScannedRoms.value = romsKey
        Main.invoke {
            try {
                val collected = linkedMapOf<String, GameInfo>() // URI → info, preserves first-seen order
                for (dirUri in romsUriStrings) {
                    val uri = try { Uri.parse(dirUri) } catch (_: Exception) { null } ?: continue
                    // All-files build: walk the folder by raw path (the SAF grant is
                    // evicted while All-Files Access is held). Falls back to SAF when
                    // the tree URI can't be resolved to a real path.
                    if (allFilesRomMode()) {
                        val root = Main.resolveTreeUriToPosix(dirUri)?.let { File(it) }
                        if (root != null && root.isDirectory) {
                            scanTreeRawInto(root, collected, 0)
                            continue
                        }
                    }
                    val tree = DocumentFile.fromTreeUri(context, uri) ?: continue
                    scanTreeInto(context, tree, collected, 0)
                }
                val sorted = collected.values.sortedBy { it.title.lowercase() }
                games.clear()
                games.addAll(sorted)
                saveCache(context, romsKey, sorted)
            } catch (e: Exception) {
                scanError.value = "Scan failed: ${e.message}"
            } finally {
                scanning.value = false
            }
        }
    }

    /**
     * Recursively walk a DocumentFile tree, adding every game-image file to
     * [collected] (keyed by URI string, first-seen wins).
     *
     * Two robustness fixes vs the old flat top-level-only walk, which silently
     * hid games a tester saw in AetherSX2:
     *  - **Recursion.** Many users keep one game per subfolder; Aether scans
     *    subdirectories, we didn't, so those titles never appeared. Depth-capped
     *    ([MAX_SCAN_DEPTH]) to bound SAF query cost / guard against odd nesting.
     *  - **No `isFile()` gate.** `DocumentFile.isFile()` returns false whenever
     *    the SAF provider reports an empty MIME type, which some providers do
     *    inconsistently for unregistered extensions like `.chd` — dropping
     *    individual files in the *same* folder with no visible pattern. We now
     *    recurse on directories and otherwise accept anything whose extension is
     *    a known game container, regardless of the reported MIME.
     */
    private fun scanTreeInto(
        context: Context,
        dir: DocumentFile,
        collected: MutableMap<String, GameInfo>,
        depth: Int,
    ) {
        if (depth > MAX_SCAN_DEPTH) return
        val entries = try { dir.listFiles() } catch (_: Exception) { return }
        for (f in entries) {
            if (f.isDirectory) {
                scanTreeInto(context, f, collected, depth + 1)
                continue
            }
            val name = f.name ?: continue
            val ext = name.substringAfterLast('.', "").lowercase()
            if (ext !in GAME_EXTENSIONS) continue

            // ISO9660 probe (PS2 BOOT2 + PS1 BOOT). Native returns a
            // "<platform>:<serial>" tag — e.g. "ps1:SLUS-00713" — when
            // SYSTEM.CNF is parseable. Compressed formats (cso/zso/gz) fall
            // through to filename. Raw-sector formats (.img/.mdf/.nrg/.dump)
            // use the same plain-ISO path in native (2352/16 and 2352/24
            // fallbacks if 2048/0 fails).
            val rawProbe = when (ext) {
                "iso", "bin", "chd", "img", "mdf", "nrg", "dump" ->
                    probeDiscSerial(context, f.uri)
                else -> null
            }
            val (probeSerial, probePlatform) = parseProbeResult(rawProbe)
            val (titleFromName, serialFromName) = FilenameParser.parse(name)
            val finalSerial = probeSerial ?: serialFromName
            val finalPlatform = probePlatform ?: GamePlatform.PS2
            // PCSX2 gamedb returns 0..6 (Unknown / Nothing / Intro / Menu /
            // InGame / Playable / Perfect). Map 0,1 → 0 stars; 2..6 → 1..5
            // stars. PS1 typically misses the PS2-only gamedb and stays 0.
            val compatRaw = if (finalSerial != null)
                NativeApp.getCompatibilityForSerial(finalSerial) else 0
            val compatStars = (compatRaw - 1).coerceIn(0, 5)
            val info = GameInfo(
                uri = f.uri,
                title = titleFromName,
                serial = finalSerial,
                compatibility = compatStars,
                extension = ext.uppercase(),
                platform = finalPlatform,
            )
            collected.putIfAbsent(f.uri.toString(), info)
        }
    }

    /** Split a "ps1:SLUS-00713" / "ps2:SLUS-20312" probe return into
     *  (serial, platform). Untagged input (legacy native or filename-only)
     *  falls back to (input, null). null/empty → (null, null). */
    private fun parseProbeResult(raw: String?): Pair<String?, GamePlatform?> {
        if (raw.isNullOrEmpty()) return null to null
        val colon = raw.indexOf(':')
        if (colon <= 0) return raw to null
        val tag = raw.substring(0, colon)
        val serial = raw.substring(colon + 1)
        return serial to GamePlatform.fromKey(tag)
    }

    /** Open `uri` for read, hand the fd to native, get the
     *  "<platform>:<serial>" back. */
    private fun probeDiscSerial(context: Context, uri: Uri): String? {
        return try {
            val pfd = context.contentResolver.openFileDescriptor(uri, "r") ?: return null
            val fd = pfd.detachFd()
            NativeApp.getGameSerialFromFd(fd) // consumes fd
        } catch (_: Exception) {
            null
        }
    }

    /** Raw-path counterpart of [scanTreeInto] for the all-files build. Walks the
     *  folder with java.io.File and tags each game with a file:// URI so the
     *  launch/probe paths open it directly instead of via the SAF FD bridge. */
    private fun scanTreeRawInto(
        dir: File,
        collected: MutableMap<String, GameInfo>,
        depth: Int,
    ) {
        if (depth > MAX_SCAN_DEPTH) return
        val entries = try { dir.listFiles() } catch (_: Exception) { null } ?: return
        for (f in entries) {
            if (f.isDirectory) {
                scanTreeRawInto(f, collected, depth + 1)
                continue
            }
            val name = f.name
            val ext = name.substringAfterLast('.', "").lowercase()
            if (ext !in GAME_EXTENSIONS) continue
            val fileUri = Uri.fromFile(f)
            val rawProbe = when (ext) {
                "iso", "bin", "chd", "img", "mdf", "nrg", "dump" -> probeDiscSerialRaw(f)
                else -> null
            }
            val (probeSerial, probePlatform) = parseProbeResult(rawProbe)
            val (titleFromName, serialFromName) = FilenameParser.parse(name)
            val finalSerial = probeSerial ?: serialFromName
            val finalPlatform = probePlatform ?: GamePlatform.PS2
            val compatRaw = if (finalSerial != null)
                NativeApp.getCompatibilityForSerial(finalSerial) else 0
            val compatStars = (compatRaw - 1).coerceIn(0, 5)
            collected.putIfAbsent(
                fileUri.toString(),
                GameInfo(
                    uri = fileUri,
                    title = titleFromName,
                    serial = finalSerial,
                    compatibility = compatStars,
                    extension = ext.uppercase(),
                    platform = finalPlatform,
                ),
            )
        }
    }

    /** Open a real file for read and hand the fd to native for the serial probe. */
    private fun probeDiscSerialRaw(file: File): String? {
        return try {
            val pfd = ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY)
            NativeApp.getGameSerialFromFd(pfd.detachFd()) // consumes fd
        } catch (_: Exception) {
            null
        }
    }

    // ---------------- Prefs cache ----------------

    private fun saveCache(context: Context, romsKey: String, list: List<GameInfo>) {
        val arr = JSONArray()
        for (g in list) {
            arr.put(JSONObject().apply {
                put("uri", g.uri.toString())
                put("title", g.title)
                put("serial", g.serial ?: JSONObject.NULL)
                put("compat", g.compatibility)
                put("ext", g.extension)
                put("platform", g.platform.key)
            })
        }
        Main.prefs.edit()
            .putString("gamesCacheKey", romsKey)
            .putString("gamesCache", arr.toString())
            .apply()
    }

    private fun loadCache(context: Context): Pair<String?, List<GameInfo>> {
        val prefs = Main.prefs
        // New key format ("|"-joined sorted dir set) under "gamesCacheKey".
        // Legacy key (single dir) was under "gamesCacheDir" — read either,
        // prefer the new one. Caller compares against the current
        // cacheKeyForDirs() so a legacy single-dir cache still matches an
        // unchanged single-dir config.
        val cachedKey = prefs.getString("gamesCacheKey", null)
            ?: prefs.getString("gamesCacheDir", null)
        val cachedJson = prefs.getString("gamesCache", null) ?: return cachedKey to emptyList()
        return try {
            val arr = JSONArray(cachedJson)
            val list = mutableListOf<GameInfo>()
            for (i in 0 until arr.length()) {
                val obj = arr.getJSONObject(i)
                val rawSerial = if (obj.isNull("serial")) null else obj.optString("serial").takeIf { it.isNotEmpty() }
                list.add(GameInfo(
                    uri = Uri.parse(obj.getString("uri")),
                    title = obj.getString("title"),
                    serial = rawSerial,
                    compatibility = obj.optInt("compat", 0),
                    extension = obj.optString("ext", "").ifEmpty {
                        obj.getString("uri").substringAfterLast('.', "").uppercase()
                    },
                    platform = GamePlatform.fromKey(
                        if (obj.isNull("platform")) null else obj.optString("platform")
                    ),
                ))
            }
            cachedKey to list
        } catch (_: Exception) {
            cachedKey to emptyList()
        }
    }
}
