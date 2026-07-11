package com.armsx2.ui.home

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.gestures.scrollBy
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
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
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.layout.layout
import androidx.compose.ui.platform.LocalContext
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

    LaunchedEffect(directories, nativeReady) { viewModel.load(directories, nativeReady) }
    DisposableEffect(viewModel, onOpenMenu) {
        HomeInputController.bind(viewModel, onOpenMenu)
        onDispose { HomeInputController.unbind(viewModel) }
    }

    CompositionLocalProvider(LocalCustomCoverMap provides customCoverMap) {
    ArmsBackdrop {
        BoxWithConstraints(modifier.fillMaxSize()) {
            val compact = maxWidth < 600.dp
            val columns = if (state.layout == LibraryLayout.List) {
                GridCells.Fixed(1)
            } else {
                GridCells.Adaptive(if (compact) 104.dp else 118.dp)
            }
            val estimatedColumns = if (state.layout == LibraryLayout.List) 1 else (maxWidth.value / if (compact) 112f else 128f).toInt().coerceAtLeast(1)
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

            LazyVerticalGrid(
                columns = columns,
                state = gridState,
                modifier = Modifier.fillMaxSize(),
                contentPadding = PaddingValues(start = 8.dp, end = 8.dp, bottom = 16.dp),
                horizontalArrangement = Arrangement.spacedBy(9.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                item(span = { GridItemSpan(maxLineSpan) }) {
                    ArmsTopBar(
                        title = str("games.section.library"),
                        subtitle = if (state.scanning) str("games.scanningRoms") else state.allGames.size.toString(),
                        leading = { RoundAction("☰", str("games.nav.library"), onOpenMenu) },
                        actions = {
                            RoundAction("↻", str("games.card.refresh"), viewModel::refresh)
                            Box {
                                RoundAction("⇅", str("games.toolbar.recent"), { sortMenu = true })
                                SortMenu(sortMenu, state.sort, viewModel::setSort) { sortMenu = false }
                            }
                            RoundAction(
                                if (state.layout == LibraryLayout.Grid) "☷" else "▦",
                                str("games.toolbar.rows"),
                                viewModel::toggleLayout,
                            )
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
                            SectionTitle(str("games.section.recentlyPlayed"))
                            Spacer(Modifier.height(9.dp))
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

                if (!state.initialized) {
                    item(span = { GridItemSpan(maxLineSpan) }) {
                        Box(Modifier.fillMaxWidth().height(240.dp), contentAlignment = Alignment.Center) {
                            CircularProgressIndicator()
                        }
                    }
                } else if (state.visibleGames.isEmpty()) {
                    emptyLibrary(state.query.isBlank())
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
            HomeSort.Compatibility to str("tab.fixes"),
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
private fun GameCover(game: GameInfo, modifier: Modifier = Modifier) {
    val context = LocalContext.current
    val customCoverMap = LocalCustomCoverMap.current
    val custom = remember(game.uri, customCoverMap) { CustomCovers.matchIn(customCoverMap, game) }
    val model = custom ?: game.coverUrl
    val request = remember(model) {
        ImageRequest.Builder(context)
            .data(model)
            .size(360, 500)
            .precision(Precision.INEXACT)
            .allowHardware(true)
            .crossfade(false)
            .build()
    }
    Box(modifier.clip(RoundedCornerShape(12.dp))) {
        CoverPlaceholder(game.title)
        if (model != null) {
            AsyncImage(
                model = request,
                contentDescription = game.title,
                modifier = Modifier.fillMaxSize(),
                contentScale = ContentScale.Crop,
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

    fun bind(viewModel: HomeViewModel, onOpenMenu: () -> Unit) {
        owner = viewModel
        openMenu = onOpenMenu
    }

    fun unbind(viewModel: HomeViewModel) {
        if (owner === viewModel) {
            owner = null
            openMenu = null
            scrollVelocity.floatValue = 0f
        }
    }

    fun setColumnCount(value: Int) { columns = value.coerceAtLeast(1) }
    fun active(): Boolean = owner != null

    fun move(dx: Int, dy: Int): Boolean {
        val viewModel = owner ?: return false
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
        openMenu?.invoke() ?: return false
        return true
    }
}
