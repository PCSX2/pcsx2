package com.armsx2.ui.home

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.clickable
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.gestures.scrollBy
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.calculateEndPadding
import androidx.compose.foundation.layout.calculateStartPadding
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.GridItemSpan
import androidx.compose.foundation.lazy.grid.LazyGridScope
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.itemsIndexed
import androidx.compose.foundation.lazy.grid.rememberLazyGridState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import com.armsx2.CoverArtStyle
import com.armsx2.R
import androidx.compose.ui.layout.layout
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import coil.compose.AsyncImage
import coil.request.ImageRequest
import coil.size.Precision
import com.armsx2.CustomCovers
import com.armsx2.GameInfo
import com.armsx2.i18n.str
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsTopBar
import com.armsx2.ui.common.EmptyState
import com.armsx2.ui.common.RoundAction
import com.armsx2.ui.common.SearchField
import com.armsx2.ui.common.SectionTitle
import com.armsx2.ui.common.StatusChip
import kotlin.math.abs

private val LocalCustomCoverMap = staticCompositionLocalOf<Map<String, java.io.File>> { emptyMap() }

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun HomeScreen(
    onOpenMenu: () -> Unit,
    onOpenGameSettings: (GameInfo) -> Unit,
    modifier: Modifier = Modifier,
    viewModel: HomeViewModel = viewModel(),
) {
    val state = viewModel.state.value
    val directories = MainActivityRuntime.romsDirs.value
    val nativeReady = MainActivityRuntime.nativeReady.value
    val context = LocalContext.current
    val coverVersion = CustomCovers.version.value
    val customCoverMap = remember(coverVersion) { CustomCovers.loadAll(context) }
    var sortMenu by remember { mutableStateOf(false) }
    var menuGame by remember { mutableStateOf<GameInfo?>(null) }
    // #9 custom library background — inert until the user picks an image.
    LaunchedEffect(Unit) { LibraryBackground.ensureLoaded(); CoverArtStyle.load() }
    var bgMenu by remember { mutableStateOf(false) }
    val backgroundPicker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { picked ->
        picked?.let { LibraryBackground.set(context, it) }
    }

    LaunchedEffect(directories, nativeReady) { viewModel.load(directories, nativeReady) }
    DisposableEffect(viewModel, onOpenMenu) {
        HomeInputController.bind(viewModel, onOpenMenu)
        onDispose { HomeInputController.unbind(viewModel) }
    }

    CompositionLocalProvider(LocalCustomCoverMap provides customCoverMap) {
    ArmsBackdrop {
        BoxWithConstraints(modifier.fillMaxSize()) {
            val libraryBg = LibraryBackground.uri.value
            if (libraryBg != null) {
                AsyncImage(
                    model = ImageRequest.Builder(context).data(libraryBg).crossfade(true).build(),
                    contentDescription = null,
                    modifier = Modifier.fillMaxSize(),
                    contentScale = ContentScale.Crop,
                )
            } else {
                // Default backdrop: the ARMSX2 tower logo, centred like the old UI.
                // Its baked-in dark radial blends with the app background, so Fit
                // (letterboxed) reads as a proper centred hero rather than a crop.
                Image(
                    painter = painterResource(R.drawable.savetowerforeground),
                    contentDescription = null,
                    modifier = Modifier.fillMaxSize(),
                    contentScale = ContentScale.Fit,
                )
            }
            // Scrim so covers and text stay readable over the backdrop.
            Box(
                Modifier.fillMaxSize().background(
                    Brush.verticalGradient(
                        listOf(
                            MaterialTheme.colorScheme.background.copy(alpha = 0.55f),
                            MaterialTheme.colorScheme.background.copy(alpha = 0.80f),
                        ),
                    ),
                ),
            )
            val compact = maxWidth < 600.dp
            val columns = if (state.layout == LibraryLayout.Grid) {
                GridCells.Adaptive(if (compact) 104.dp else 118.dp)
            } else {
                // List and Shelf are full-width rows.
                GridCells.Fixed(1)
            }
            val estimatedColumns = if (state.layout == LibraryLayout.Grid) (maxWidth.value / if (compact) 112f else 128f).toInt().coerceAtLeast(1) else 1
            LaunchedEffect(estimatedColumns) { HomeInputController.setColumnCount(estimatedColumns) }

            val gridState = rememberLazyGridState()
            val density = LocalDensity.current
            LaunchedEffect(gridState) {
                var lastFrame = withFrameNanos { it }
                while (true) {
                    val frame = withFrameNanos { it }
                    val dt = ((frame - lastFrame).coerceAtMost(50_000_000L)).toFloat() / 1_000_000_000f
                    lastFrame = frame
                    val velocity = HomeInputController.scrollVelocity.floatValue
                    if (abs(velocity) > 0.08f) {
                        val pxPerSecond = with(density) { 1500.dp.toPx() }
                        gridState.scrollBy(velocity * pxPerSecond * dt)
                    }
                }
            }
            LaunchedEffect(state.selectedIndex, state.visibleGames.size) {
                if (state.visibleGames.isNotEmpty() && state.query.isNotBlank()) {
                    gridState.animateScrollToItem(state.selectedIndex.coerceAtLeast(0) + 2)
                }
            }

            // Keep covers/text out of the display's unsafe edge (cutout / rounded
            // corner) — add the cutout inset to the side padding. The full-bleed
            // shelf/recent rows only negate the base 8dp, so they stop at the safe
            // edge rather than bleeding under the cutout.
            val cutout = WindowInsets.displayCutout.asPaddingValues()
            val ld = LocalLayoutDirection.current
            LazyVerticalGrid(
                columns = columns,
                state = gridState,
                modifier = Modifier.fillMaxSize(),
                contentPadding = PaddingValues(
                    start = 8.dp + cutout.calculateStartPadding(ld),
                    end = 8.dp + cutout.calculateEndPadding(ld),
                    bottom = 16.dp,
                ),
                horizontalArrangement = Arrangement.spacedBy(9.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                item(span = { GridItemSpan(maxLineSpan) }) {
                    // Register the toolbar button actions (left→right order) so the
                    // controller's toolbar zone can fire them, and read the highlight
                    // state so the focused button lights up.
                    HomeInputController.setToolbarActions(
                        listOf(
                            onOpenMenu,
                            { viewModel.refresh() },
                            { sortMenu = true },
                            { viewModel.toggleLayout() },
                            { CoverArtStyle.set(!CoverArtStyle.use3d.value) },
                            { bgMenu = true },
                        ),
                    )
                    val tb = HomeInputController.toolbarFocused.value
                    val tbi = HomeInputController.toolbarIndex.intValue
                    ArmsTopBar(
                        title = str("games.section.library"),
                        subtitle = if (state.scanning) str("games.scanningRoms") else state.allGames.size.toString(),
                        leading = { RoundAction("☰", str("games.nav.library"), onOpenMenu, selected = tb && tbi == 0) },
                        actions = {
                            RoundAction("↻", str("games.card.refresh"), viewModel::refresh, selected = tb && tbi == 1)
                            Box {
                                RoundAction("⇅", str("games.toolbar.recent"), { sortMenu = true }, selected = tb && tbi == 2)
                                SortMenu(sortMenu, state.sort, viewModel::setSort) { sortMenu = false }
                            }
                            RoundAction(
                                when (state.layout) {
                                    LibraryLayout.Grid -> "☷"
                                    LibraryLayout.List -> "▦"
                                    LibraryLayout.Shelf -> "▤"
                                },
                                str("games.toolbar.rows"),
                                viewModel::toggleLayout,
                                selected = tb && tbi == 3,
                            )
                            // 3D box-art covers vs flat 2D scans (xlenore covers/3d).
                            // Shows the current mode (like the layout toggle), tap to flip.
                            RoundAction(
                                if (CoverArtStyle.use3d.value) "3D" else "2D",
                                str("games.toolbar.covers3d"),
                                { CoverArtStyle.set(!CoverArtStyle.use3d.value) },
                                selected = tb && tbi == 4,
                            )
                            Box {
                                RoundAction("▧", str("games.toolbar.background"), { bgMenu = true }, selected = tb && tbi == 5)
                                DropdownMenu(expanded = bgMenu, onDismissRequest = { bgMenu = false }) {
                                    DropdownMenuItem(
                                        text = { Text(str("games.background.choose")) },
                                        onClick = { bgMenu = false; backgroundPicker.launch(arrayOf("image/*")) },
                                    )
                                    if (LibraryBackground.uri.value != null) {
                                        DropdownMenuItem(
                                            text = { Text(str("games.background.clear")) },
                                            onClick = { bgMenu = false; LibraryBackground.clear() },
                                        )
                                    }
                                }
                            }
                        },
                        horizontalPadding = 0.dp,
                    )
                }
                if (state.initialized) {
                    item(span = { GridItemSpan(maxLineSpan) }) {
                        SearchField(
                            value = state.query,
                            onValueChange = viewModel::setQuery,
                            placeholder = str("games.search.placeholder"),
                            modifier = Modifier.fillMaxWidth(),
                        )
                    }
                }

                if (state.recentGames.isNotEmpty() && state.query.isBlank()) {
                    item(span = { GridItemSpan(maxLineSpan) }) {
                        Column {
                            // In shelf view nudge the header right so it lines up with
                            // the first cover's left edge (the shelf's 12dp inset).
                            SectionTitle(
                                str("games.section.recentlyPlayed"),
                                modifier = Modifier.padding(
                                    start = if (state.layout == LibraryLayout.Shelf) 4.dp else 0.dp,
                                ),
                            )
                            Spacer(Modifier.height(9.dp))
                            if (state.layout == LibraryLayout.Shelf) {
                                // Same frosted-glass plank as All Games (bagas: one
                                // shelf design everywhere).
                                GameShelf(
                                    games = state.recentGames.take(10),
                                    shelfRes = R.drawable.shelf_frosted,
                                    coverWidth = if (compact) 84.dp else 100.dp,
                                    scroll = true,
                                    onLaunch = { viewModel.launch(it) },
                                    modifier = Modifier.layout { measurable, constraints ->
                                        val edge = 8.dp.roundToPx()
                                        val placeable = measurable.measure(
                                            constraints.copy(
                                                minWidth = constraints.maxWidth + edge * 2,
                                                maxWidth = constraints.maxWidth + edge * 2,
                                            ),
                                        )
                                        layout(constraints.maxWidth, placeable.height) { placeable.placeRelative(-edge, 0) }
                                    },
                                )
                            } else {
                                LazyRow(
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .layout { measurable, constraints ->
                                            val edge = 8.dp.roundToPx()
                                            val placeable = measurable.measure(
                                                constraints.copy(
                                                    minWidth = constraints.minWidth + edge * 2,
                                                    maxWidth = constraints.maxWidth + edge * 2,
                                                ),
                                            )
                                            layout(constraints.maxWidth, placeable.height) {
                                                placeable.placeRelative(-edge, 0)
                                            }
                                        },
                                    horizontalArrangement = Arrangement.spacedBy(10.dp),
                                    contentPadding = PaddingValues(horizontal = 8.dp),
                                ) {
                                    items(state.recentGames.take(10), key = { it.uri.toString() }) { game ->
                                        RecentGameCard(
                                            game = game,
                                            onClick = { viewModel.launch(game) },
                                            onDetails = { menuGame = game },
                                        )
                                    }
                                }
                            }
                        }
                    }
                }

                // Separate the Recently Played shelf from the full library with an
                // "All Games" header so the two rows don't run together.
                if (state.recentGames.isNotEmpty() && state.query.isBlank() && state.initialized && state.visibleGames.isNotEmpty()) {
                    item(span = { GridItemSpan(maxLineSpan) }) {
                        Column {
                            Spacer(Modifier.height(20.dp))
                            SectionTitle(
                                str("games.section.allGames"),
                                modifier = Modifier.padding(
                                    start = if (state.layout == LibraryLayout.Shelf) 4.dp else 0.dp,
                                ),
                            )
                            Spacer(Modifier.height(9.dp))
                        }
                    }
                }

                if (!state.initialized) {
                    item(span = { GridItemSpan(maxLineSpan) }) {
                        Box(Modifier.fillMaxWidth().height(240.dp), contentAlignment = Alignment.Center) {
                            CircularProgressIndicator()
                        }
                    }
                } else if (state.visibleGames.isEmpty()) {
                    emptyLibrary(state.query.isBlank())
                } else if (state.layout == LibraryLayout.Shelf) {
                    // Fill each plank: chunk by how many covers fit the shelf width.
                    val shelfCoverW = if (compact) 84.dp else 100.dp
                    val perShelf = (maxWidth.value / (shelfCoverW.value + 20f)).toInt().coerceIn(3, 8)
                    val shelfRows = state.visibleGames.chunked(perShelf)
                    items(
                        shelfRows.size,
                        span = { GridItemSpan(maxLineSpan) },
                        key = { "shelf_$it" },
                    ) { rowIndex ->
                        GameShelf(
                            games = shelfRows[rowIndex],
                            shelfRes = R.drawable.shelf_frosted,
                            coverWidth = shelfCoverW,
                            scroll = false,
                            // Every row lays out on the same perShelf-slot grid, so a
                            // short last row keeps its covers packed left in sequence.
                            slotsPerRow = perShelf,
                            onLaunch = { viewModel.launch(it) },
                            // Bleed past the grid's 8dp side padding so the glass shelf
                            // reaches both screen edges instead of floating inset.
                            modifier = Modifier.layout { measurable, constraints ->
                                val edge = 8.dp.roundToPx()
                                val placeable = measurable.measure(
                                    constraints.copy(
                                        minWidth = constraints.maxWidth + edge * 2,
                                        maxWidth = constraints.maxWidth + edge * 2,
                                    ),
                                )
                                layout(constraints.maxWidth, placeable.height) { placeable.placeRelative(-edge, 0) }
                            },
                        )
                    }
                } else {
                    itemsIndexed(
                        state.visibleGames,
                        key = { _, game -> game.uri.toString() },
                        contentType = { _, _ -> state.layout.name },
                    ) { index, game ->
                        if (state.layout == LibraryLayout.Grid) {
                            GameGridCard(
                                game = game,
                                selected = index == state.selectedIndex,
                                onSelect = { viewModel.setSelection(index) },
                                onLaunch = { viewModel.launch(game) },
                                onDetails = { menuGame = game },
                            )
                        } else {
                            GameListCard(
                                game = game,
                                selected = index == state.selectedIndex,
                                onClick = { viewModel.setSelection(index); viewModel.launch(game) },
                                onDetails = { menuGame = game },
                            )
                        }
                    }
                }
            }
        }
    }
    }

    menuGame?.let { game ->
        ModalBottomSheet(onDismissRequest = { menuGame = null }) {
            Column(
                Modifier.fillMaxWidth().padding(start = 8.dp, end = 8.dp, bottom = 20.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Text(
                    game.title,
                    style = MaterialTheme.typography.titleLarge,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.padding(horizontal = 8.dp, vertical = 6.dp),
                )
                GameMenuAction("▶", str("action.play")) {
                    menuGame = null
                    viewModel.launch(game)
                }
                GameMenuAction("⚙", str("action.settings")) {
                    menuGame = null
                    onOpenGameSettings(game)
                }
            }
        }
    }
}

@Composable
private fun GameMenuAction(glyph: String, label: String, onClick: () -> Unit) {
    Surface(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.surfaceVariant,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.45f)),
    ) {
        Row(
            Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 14.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(14.dp),
        ) {
            Text(glyph, color = MaterialTheme.colorScheme.primary, fontSize = 20.sp)
            Text(label, style = MaterialTheme.typography.titleMedium)
        }
    }
}

@Composable
private fun SortMenu(expanded: Boolean, selected: HomeSort, onSort: (HomeSort) -> Unit, dismiss: () -> Unit) {
    DropdownMenu(expanded = expanded, onDismissRequest = dismiss) {
        listOf(
            HomeSort.Title to str("games.section.library"),
            HomeSort.RecentlyPlayed to str("games.section.recentlyPlayed"),
        ).forEach { (sort, label) ->
            DropdownMenuItem(
                text = { Text(label, fontWeight = if (sort == selected) FontWeight.Bold else FontWeight.Normal) },
                onClick = { onSort(sort); dismiss() },
            )
        }
    }
}

private fun LazyGridScope.emptyLibrary(noFolders: Boolean) {
    item(span = { GridItemSpan(maxLineSpan) }) {
        EmptyState(
            title = if (noFolders) str("games.empty.noFolders.title") else str("games.search.placeholder"),
            message = if (noFolders) str("games.empty.noFolders.body") else str("games.search.hint"),
            actionLabel = if (noFolders) str("games.toolbar.setup") else null,
            onAction = if (noFolders) MainActivityRuntime::reopenSetup else null,
            modifier = Modifier.fillMaxWidth().height(260.dp),
        )
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun GameGridCard(
    game: GameInfo,
    selected: Boolean,
    onSelect: () -> Unit,
    onLaunch: () -> Unit,
    onDetails: () -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .combinedClickable(onClick = { onSelect(); onLaunch() }, onLongClick = onDetails),
    ) {
        GameCover(
            game,
            Modifier
                .fillMaxWidth()
                .aspectRatio(0.72f)
                .border(
                    BorderStroke(
                        if (selected) 2.dp else 1.dp,
                        if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline.copy(alpha = 0.42f),
                    ),
                    RoundedCornerShape(12.dp),
                ),
        )
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun GameListCard(game: GameInfo, selected: Boolean, onClick: () -> Unit, onDetails: () -> Unit) {
    Surface(
        modifier = Modifier.fillMaxWidth().combinedClickable(onClick = onClick, onLongClick = onDetails),
        shape = RoundedCornerShape(15.dp),
        color = MaterialTheme.colorScheme.surface,
        border = BorderStroke(
            if (selected) 2.dp else 1.dp,
            if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline.copy(alpha = 0.42f),
        ),
    ) {
        Row(Modifier.padding(7.dp), verticalAlignment = Alignment.CenterVertically) {
            GameCover(game, Modifier.width(54.dp).aspectRatio(0.72f))
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text(game.title, style = MaterialTheme.typography.titleSmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
                Spacer(Modifier.height(5.dp))
                GameMetadata(game)
            }
            Text("▶", color = MaterialTheme.colorScheme.primary, modifier = Modifier.padding(10.dp))
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun RecentGameCard(game: GameInfo, onClick: () -> Unit, onDetails: () -> Unit) {
    Column(
        modifier = Modifier
            .width(102.dp)
            .combinedClickable(onClick = onClick, onLongClick = onDetails),
    ) {
        GameCover(
            game,
            Modifier.fillMaxWidth().aspectRatio(0.72f).border(
                1.dp,
                MaterialTheme.colorScheme.outline.copy(alpha = 0.42f),
                RoundedCornerShape(12.dp),
            ),
        )
        Spacer(Modifier.height(5.dp))
        Text(game.title, style = MaterialTheme.typography.labelMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
    }
}

@Composable
private fun GameMetadata(game: GameInfo) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(5.dp)) {
        StatusChip(game.extension.ifBlank { game.platform.key.uppercase() })
        game.regionFlag?.let { Text(it, fontSize = 13.sp) }
        if (game.compatibility > 0) {
            Text("★".repeat(game.compatibility), color = Color(0xFFFFC857), fontSize = 9.sp, maxLines = 1)
        }
    }
}

@Composable
private fun GameCover(
    game: GameInfo,
    modifier: Modifier = Modifier,
    cornerRadius: Dp = 12.dp,
    // Fit (not Crop) everywhere so the whole box art shows — Crop trims the top of
    // covers whose source art is a touch taller than the 0.7 slot.
    contentScale: ContentScale = ContentScale.Fit,
) {
    val context = LocalContext.current
    // Read the 3D-cover flag explicitly (not just via game.coverUrl, which is
    // skipped when a custom cover wins) so EVERY card — the Recently Played shelf
    // included — is subscribed and re-resolves when the toolbar toggle flips.
    val use3d = CoverArtStyle.use3d.value
    val customCoverMap = LocalCustomCoverMap.current
    val custom = remember(game.uri, customCoverMap) { CustomCovers.matchIn(customCoverMap, game) }
    val model = custom ?: game.coverUrl
    val request = remember(model, use3d) {
        ImageRequest.Builder(context)
            .data(model)
            .size(360, 500)
            .precision(Precision.INEXACT)
            .allowHardware(true)
            .crossfade(false)
            .build()
    }
    Box(modifier.clip(RoundedCornerShape(cornerRadius))) {
        if (model == null) {
            CoverPlaceholder(game.title)
        } else {
            // No fill behind the art: 3D box-art PNGs are transparent around the
            // angled case, and any backing shows as a dark/coloured "notch" at the
            // top. Keeping it transparent lets the case sit directly on the shelf.
            AsyncImage(
                model = request,
                contentDescription = game.title,
                modifier = Modifier.fillMaxSize(),
                contentScale = contentScale,
            )
        }
    }
}

@Composable
private fun CoverPlaceholder(title: String) {
    Box(
        Modifier.fillMaxSize().background(
            Brush.linearGradient(
                listOf(MaterialTheme.colorScheme.primaryContainer, MaterialTheme.colorScheme.surfaceVariant),
            ),
        ),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            title.take(2).uppercase(),
            style = MaterialTheme.typography.headlineSmall,
            fontWeight = FontWeight.Black,
            color = MaterialTheme.colorScheme.onPrimaryContainer.copy(alpha = 0.78f),
        )
    }
}

object HomeInputController {
    private var owner: HomeViewModel? = null
    private var openMenu: (() -> Unit)? = null
    private var columns = 3
    val scrollVelocity = mutableFloatStateOf(0f)

    // Top-toolbar zone. The cover grid is data-driven (owner.selectedIndex), but the
    // toolbar (refresh / sort / layout / 2D-3D / background, plus the leading menu
    // button) sits above it and can't be part of that index space. So the toolbar is
    // a separate "zone": pressing Up from the grid's top row focuses it, Left/Right
    // move between its buttons, A fires the highlighted one, Down drops back to the
    // grid. HomeScreen registers the button actions and reads `toolbarFocused` /
    // `toolbarIndex` to draw the highlight.
    val toolbarFocused = mutableStateOf(false)
    val toolbarIndex = mutableIntStateOf(0)
    private var toolbarActions: List<() -> Unit> = emptyList()

    fun bind(viewModel: HomeViewModel, onOpenMenu: () -> Unit) {
        owner = viewModel
        openMenu = onOpenMenu
    }

    fun unbind(viewModel: HomeViewModel) {
        if (owner === viewModel) {
            owner = null
            openMenu = null
            scrollVelocity.floatValue = 0f
            toolbarFocused.value = false
        }
    }

    fun setColumnCount(value: Int) { columns = value.coerceAtLeast(1) }
    fun active(): Boolean = owner != null

    /** HomeScreen registers the toolbar button actions here, in visual left→right
     *  order (leading menu button first). */
    fun setToolbarActions(actions: List<() -> Unit>) { toolbarActions = actions }

    private fun atTopRow(): Boolean {
        val vm = owner ?: return false
        return vm.state.value.selectedIndex < columns
    }

    fun move(dx: Int, dy: Int): Boolean {
        val viewModel = owner ?: return false
        if (toolbarFocused.value) {
            when {
                dy > 0 -> toolbarFocused.value = false            // Down → back to grid
                dx != 0 && toolbarActions.isNotEmpty() ->
                    toolbarIndex.intValue = (toolbarIndex.intValue + dx).coerceIn(0, toolbarActions.lastIndex)
            }
            return true
        }
        // Up from the top cover row jumps into the toolbar.
        if (dy < 0 && atTopRow() && toolbarActions.isNotEmpty()) {
            toolbarFocused.value = true
            return true
        }
        val delta = when {
            dx < 0 -> -1
            dx > 0 -> 1
            dy < 0 -> -columns
            dy > 0 -> columns
            else -> 0
        }
        if (delta != 0) viewModel.moveSelection(delta)
        return true
    }

    fun confirm(): Boolean {
        val viewModel = owner ?: return false
        if (toolbarFocused.value) {
            toolbarActions.getOrNull(toolbarIndex.intValue)?.invoke()
            return true
        }
        val game = viewModel.selectedGame() ?: return false
        viewModel.launch(game)
        return true
    }

    fun openSelectedSettings(): Boolean {
        val game = owner?.selectedGame() ?: return false
        com.armsx2.navigation.UiNavigator.navigate(com.armsx2.navigation.AppRoute.Settings(game = game))
        return true
    }

    fun scroll(velocity: Float): Boolean {
        if (owner == null) return false
        scrollVelocity.floatValue = if (abs(velocity) > 0.08f) velocity.coerceIn(-1f, 1f) else 0f
        return true
    }

    fun back(): Boolean {
        // B while the toolbar is focused just drops back to the grid.
        if (toolbarFocused.value) {
            toolbarFocused.value = false
            return true
        }
        openMenu?.invoke() ?: return false
        return true
    }

    /** Cycle the library layout Grid → List → Shelf (bound to R1). Gives the
     *  controller direct access to all three view modes without needing to reach
     *  the touch-only toolbar toggle. */
    fun cycleLayout(): Boolean {
        val viewModel = owner ?: return false
        viewModel.toggleLayout()
        return true
    }

    /** Cycle the library sort order (bound to L1). */
    fun cycleSort(): Boolean {
        val viewModel = owner ?: return false
        val entries = HomeSort.entries
        val next = entries[(viewModel.state.value.sort.ordinal + 1) % entries.size]
        viewModel.setSort(next)
        return true
    }
}

// -- vivi's shelf library layout — covers standing on vivi's plank PNGs (frosted
//    for All Games, navy for Now Playing). Each cover sits seated on the plank's
//    top face (back-of-centre, like a book on a shelf) with shelf surface visible
//    in front of it, and a faint mirror reflection on that surface. --

@Composable
private fun GameShelf(
    games: List<GameInfo>,
    shelfRes: Int,
    coverWidth: Dp,
    scroll: Boolean,
    onLaunch: (GameInfo) -> Unit,
    modifier: Modifier = Modifier,
    slotsPerRow: Int = games.size,
) {
    val coverHeight = coverWidth / 0.7f
    // Slimmer plank to match bagas's slimmer frosted-shelf PNG (2903×200).
    val plankHeight = 84.dp
    // How far below the plank's back (top) edge the cover base sits — small, so the
    // cover rests toward the BACK of the top face with plenty of shelf in front of
    // it (not perched on the front edge). Scaled with the slimmer plank.
    val surfaceInset = 16.dp
    val reflectionHeight = coverHeight * 0.18f
    // Covers + reflection are top-anchored; the box is tall enough that the cover
    // base lands on the top face and the reflection lays over the shelf in front.
    val rowHeight = coverHeight + reflectionHeight
    Box(modifier.fillMaxWidth().height(coverHeight + plankHeight - surfaceInset)) {
        Image(
            painter = painterResource(shelfRes),
            contentDescription = null,
            // The shelf PNG carries a ~1.1% transparent/faded margin on each side, so
            // when stretched full-width its *visible* plank edge sits ~11dp inset while
            // the first/last covers reach the screen edge — making them overhang the
            // shelf's faded end. Scale the plank out ~5% horizontally so its solid
            // surface bleeds to (past) the screen edges and the covers sit on it.
            modifier = Modifier.align(Alignment.BottomCenter).fillMaxWidth().height(plankHeight)
                .graphicsLayer { scaleX = 1.05f },
            contentScale = ContentScale.FillBounds,
        )
        if (scroll) {
            LazyRow(
                modifier = Modifier.align(Alignment.TopStart).fillMaxWidth().height(rowHeight),
                // 12dp start so the first cover lines up with the section header.
                contentPadding = PaddingValues(horizontal = 12.dp),
                horizontalArrangement = Arrangement.spacedBy(16.dp),
            ) {
                items(games, key = { "shelfcard_${shelfRes}_${it.uri}" }) { game ->
                    ShelfGameCard(game, coverWidth, reflectionHeight, onLaunch)
                }
            }
        } else {
            // Covers laid left-to-right at their full-row positions. SpaceBetween
            // pins the first cover to the left padding (aligned with the section
            // header) and spreads the row across the shelf width. A short trailing
            // row is padded with invisible spacers so its covers stay packed on the
            // left in sequence instead of sprawling across the whole shelf.
            Row(
                modifier = Modifier.align(Alignment.TopStart).fillMaxWidth().height(rowHeight).padding(horizontal = 12.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                games.forEach { game -> ShelfGameCard(game, coverWidth, reflectionHeight, onLaunch) }
                repeat((slotsPerRow - games.size).coerceAtLeast(0)) {
                    Spacer(Modifier.width(coverWidth))
                }
            }
        }
    }
}

@Composable
private fun ShelfGameCard(game: GameInfo, width: Dp, reflectionHeight: Dp, onLaunch: (GameInfo) -> Unit) {
    Column(modifier = Modifier.width(width).clickable { onLaunch(game) }) {
        // Square corners in shelf view — rounding fought the 3D box-art edges. The
        // grid/cover view keeps rounded corners (GameCover's 12.dp default).
        // ContentScale.Fit shows the WHOLE cover — Crop was trimming the top off the
        // large standing covers.
        GameCover(
            game,
            Modifier.fillMaxWidth().aspectRatio(0.7f),
            cornerRadius = 0.dp,
            contentScale = ContentScale.Fit,
        )
        // A faint mirror of the cover on the shelf surface just in front of it.
        // clipToBounds keeps it to reflectionHeight — without it the full flipped
        // cover renders and bleeds down onto the row below.
        Box(Modifier.fillMaxWidth().height(reflectionHeight).clipToBounds()) {
            GameCover(
                game,
                Modifier.fillMaxWidth().aspectRatio(0.7f)
                    .graphicsLayer { scaleY = -1f; alpha = 0.18f },
                cornerRadius = 0.dp,
                contentScale = ContentScale.Fit,
            )
            // Fade the reflection out toward the front of the shelf.
            Box(Modifier.matchParentSize().background(Brush.verticalGradient(listOf(Color.Transparent, Color(0x55000000)))))
        }
    }
}

