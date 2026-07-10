package com.armsx2.ui.home

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.itemsIndexed as listItemsIndexed
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.itemsIndexed as gridItemsIndexed
import androidx.compose.foundation.lazy.grid.rememberLazyGridState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import coil.compose.SubcomposeAsyncImage
import coil.request.ImageRequest
import com.armsx2.CustomCovers
import com.armsx2.GameInfo
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsLogo
import com.armsx2.ui.common.ArmsTopBar
import com.armsx2.ui.common.EmptyState
import com.armsx2.ui.common.GlassPanel
import com.armsx2.ui.common.RoundAction
import com.armsx2.ui.common.SearchField
import com.armsx2.ui.common.SectionTitle
import com.armsx2.ui.common.StatusChip
import com.armsx2.ui.theme.Success

@Composable
fun HomeScreen(
    onOpenMenu: () -> Unit,
    onOpenSettings: () -> Unit,
    onOpenGameSettings: (GameInfo) -> Unit,
    modifier: Modifier = Modifier,
    viewModel: HomeViewModel = viewModel(),
) {
    val state = viewModel.state.value
    val directories = MainActivityRuntime.romsDirs.value
    val nativeReady = MainActivityRuntime.nativeReady.value

    LaunchedEffect(directories, nativeReady) {
        viewModel.load(directories, nativeReady)
    }
    DisposableEffect(viewModel, onOpenMenu) {
        HomeInputController.bind(viewModel, onOpenMenu)
        onDispose { HomeInputController.unbind(viewModel) }
    }

    ArmsBackdrop {
        Column(modifier.fillMaxSize()) {
            ArmsTopBar(
                title = "Game library",
                subtitle = when {
                    state.scanning -> "Scanning selected folders"
                    state.allGames.isEmpty() -> "Add a folder to begin"
                    else -> "${state.allGames.size} games ready"
                },
                leading = { RoundAction("☰", "Navigation", onOpenMenu) },
                actions = {
                    RoundAction("↻", "Refresh", viewModel::refresh)
                    RoundAction(
                        if (state.layout == LibraryLayout.Grid) "☷" else "▦",
                        "Change layout",
                        viewModel::toggleLayout,
                    )
                    RoundAction("⚙", "Settings", onOpenSettings)
                },
            )

            BoxWithConstraints(Modifier.fillMaxSize()) {
                val wide = maxWidth >= 780.dp
                if (wide) {
                    Row(
                        modifier = Modifier
                            .fillMaxSize()
                            .padding(start = 22.dp, end = 22.dp, bottom = 18.dp),
                        horizontalArrangement = Arrangement.spacedBy(18.dp),
                    ) {
                        LibrarySidebar(
                            state = state,
                            onQuery = viewModel::setQuery,
                            onSort = viewModel::setSort,
                            onRefresh = viewModel::refresh,
                            onEditFolders = MainActivityRuntime::reopenSetup,
                            modifier = Modifier.width(260.dp).fillMaxHeight(),
                        )
                        LibraryContent(
                            state = state,
                            viewModel = viewModel,
                            onOpenGameSettings = onOpenGameSettings,
                            modifier = Modifier.weight(1f).fillMaxHeight(),
                        )
                    }
                } else {
                    Column(
                        modifier = Modifier
                            .fillMaxSize()
                            .padding(horizontal = 16.dp),
                    ) {
                        SearchField(
                            value = state.query,
                            onValueChange = viewModel::setQuery,
                            modifier = Modifier.fillMaxWidth(),
                        )
                        Spacer(Modifier.height(12.dp))
                        LibraryContent(
                            state = state,
                            viewModel = viewModel,
                            onOpenGameSettings = onOpenGameSettings,
                            modifier = Modifier.weight(1f),
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun LibrarySidebar(
    state: HomeUiState,
    onQuery: (String) -> Unit,
    onSort: (HomeSort) -> Unit,
    onRefresh: () -> Unit,
    onEditFolders: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var sortMenu by remember { mutableStateOf(false) }
    GlassPanel(modifier = modifier, contentPadding = 16.dp) {
        Column(Modifier.fillMaxSize()) {
            ArmsLogo()
            Spacer(Modifier.height(20.dp))
            SearchField(state.query, onQuery, Modifier.fillMaxWidth())
            Spacer(Modifier.height(18.dp))
            SectionTitle("Browse", "Your PlayStation library")
            Spacer(Modifier.height(10.dp))
            SidebarAction(
                label = when (state.sort) {
                    HomeSort.Title -> "Title"
                    HomeSort.RecentlyPlayed -> "Recently played"
                    HomeSort.Compatibility -> "Compatibility"
                },
                value = "Sort",
                onClick = { sortMenu = true },
            )
            Box {
                DropdownMenu(expanded = sortMenu, onDismissRequest = { sortMenu = false }) {
                    DropdownMenuItem(
                        text = { Text("Title") },
                        onClick = { onSort(HomeSort.Title); sortMenu = false },
                    )
                    DropdownMenuItem(
                        text = { Text("Recently played") },
                        onClick = { onSort(HomeSort.RecentlyPlayed); sortMenu = false },
                    )
                    DropdownMenuItem(
                        text = { Text("Compatibility") },
                        onClick = { onSort(HomeSort.Compatibility); sortMenu = false },
                    )
                }
            }
            SidebarAction("Refresh library", "Scan", onRefresh)
            SidebarAction("Game folders", "Storage", onEditFolders)
            Spacer(Modifier.weight(1f))
            if (state.scanning) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    CircularProgressIndicator(Modifier.size(18.dp), strokeWidth = 2.dp)
                    Spacer(Modifier.width(10.dp))
                    Text("Updating library", style = MaterialTheme.typography.bodySmall)
                }
            } else {
                StatusChip("${state.allGames.size} titles", Success)
            }
        }
    }
}

@Composable
private fun SidebarAction(label: String, value: String, onClick: () -> Unit) {
    Surface(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
        shape = RoundedCornerShape(14.dp),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.72f),
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 13.dp, vertical = 11.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Column(Modifier.weight(1f)) {
                Text(value, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                Text(label, style = MaterialTheme.typography.labelLarge)
            }
            Text("›", fontSize = 22.sp, color = MaterialTheme.colorScheme.primary)
        }
    }
}

@Composable
private fun LibraryContent(
    state: HomeUiState,
    viewModel: HomeViewModel,
    onOpenGameSettings: (GameInfo) -> Unit,
    modifier: Modifier = Modifier,
) {
    if (state.visibleGames.isEmpty()) {
        GlassPanel(modifier.fillMaxSize()) {
            EmptyState(
                title = if (state.query.isBlank()) "Your library is empty" else "No matching games",
                message = if (state.query.isBlank()) {
                    "Select one or more folders with PS2 or PS1 images."
                } else {
                    "Try a different title, serial, or file format."
                },
                actionLabel = if (state.query.isBlank()) "Choose folders" else null,
                onAction = if (state.query.isBlank()) MainActivityRuntime::reopenSetup else null,
                modifier = Modifier.fillMaxSize(),
            )
        }
        return
    }

    Column(modifier) {
        if (state.recentGames.isNotEmpty() && state.query.isBlank() && state.sort != HomeSort.RecentlyPlayed) {
            SectionTitle("Continue playing", "Recently launched games")
            Spacer(Modifier.height(10.dp))
            LazyRow(
                horizontalArrangement = Arrangement.spacedBy(12.dp),
                contentPadding = PaddingValues(end = 8.dp),
            ) {
                items(state.recentGames.take(8), key = { it.uri.toString() }) { game ->
                    RecentGameCard(
                        game = game,
                        onClick = { viewModel.launch(game) },
                        onDetails = { onOpenGameSettings(game) },
                    )
                }
            }
            Spacer(Modifier.height(18.dp))
        }

        Row(verticalAlignment = Alignment.CenterVertically) {
            SectionTitle(
                if (state.query.isBlank()) "All games" else "Search results",
                "${state.visibleGames.size} shown",
                Modifier.weight(1f),
            )
            state.error?.let { StatusChip(it, MaterialTheme.colorScheme.error) }
        }
        Spacer(Modifier.height(10.dp))

        if (state.layout == LibraryLayout.Grid) {
            GameGrid(state, viewModel, onOpenGameSettings, Modifier.weight(1f))
        } else {
            GameList(state, viewModel, onOpenGameSettings, Modifier.weight(1f))
        }
    }
}

@Composable
private fun GameGrid(
    state: HomeUiState,
    viewModel: HomeViewModel,
    onDetails: (GameInfo) -> Unit,
    modifier: Modifier,
) {
    val gridState = rememberLazyGridState()
    LaunchedEffect(state.selectedIndex) {
        if (state.visibleGames.isNotEmpty()) gridState.animateScrollToItem(state.selectedIndex)
    }
    LazyVerticalGrid(
        columns = GridCells.Adaptive(142.dp),
        state = gridState,
        modifier = modifier,
        horizontalArrangement = Arrangement.spacedBy(12.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
        contentPadding = PaddingValues(bottom = 20.dp),
    ) {
        gridItemsIndexed(state.visibleGames, key = { _, game -> game.uri.toString() }) { index, game ->
            GameGridCard(
                game = game,
                selected = index == state.selectedIndex,
                onSelect = { viewModel.setSelection(index) },
                onLaunch = { viewModel.launch(game) },
                onDetails = { onDetails(game) },
            )
        }
    }
}

@Composable
private fun GameList(
    state: HomeUiState,
    viewModel: HomeViewModel,
    onDetails: (GameInfo) -> Unit,
    modifier: Modifier,
) {
    LazyColumn(
        modifier = modifier,
        verticalArrangement = Arrangement.spacedBy(8.dp),
        contentPadding = PaddingValues(bottom = 20.dp),
    ) {
        listItemsIndexed(state.visibleGames, key = { _, game -> game.uri.toString() }) { index, game ->
            GameListCard(
                game = game,
                selected = index == state.selectedIndex,
                onClick = { viewModel.setSelection(index); viewModel.launch(game) },
                onDetails = { onDetails(game) },
            )
        }
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
            .widthIn(min = 132.dp, max = 190.dp)
            .clip(RoundedCornerShape(18.dp))
            .background(MaterialTheme.colorScheme.surface)
            .border(
                BorderStroke(
                    if (selected) 2.dp else 1.dp,
                    if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline.copy(alpha = 0.52f),
                ),
                RoundedCornerShape(18.dp),
            )
            .combinedClickable(
                onClick = { onSelect(); onLaunch() },
                onLongClick = onDetails,
            )
            .padding(8.dp),
    ) {
        GameCover(game, Modifier.fillMaxWidth().aspectRatio(0.72f))
        Spacer(Modifier.height(9.dp))
        Text(
            game.title,
            style = MaterialTheme.typography.titleSmall,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis,
        )
        Spacer(Modifier.height(5.dp))
        GameMetadata(game)
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun GameListCard(
    game: GameInfo,
    selected: Boolean,
    onClick: () -> Unit,
    onDetails: () -> Unit,
) {
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .border(
                if (selected) 2.dp else 1.dp,
                if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline.copy(alpha = 0.48f),
                RoundedCornerShape(18.dp),
            )
            .combinedClickable(onClick = onClick, onLongClick = onDetails),
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.surface,
    ) {
        Row(Modifier.padding(9.dp), verticalAlignment = Alignment.CenterVertically) {
            GameCover(game, Modifier.width(58.dp).aspectRatio(0.72f))
            Spacer(Modifier.width(14.dp))
            Column(Modifier.weight(1f)) {
                Text(game.title, style = MaterialTheme.typography.titleMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
                Spacer(Modifier.height(5.dp))
                GameMetadata(game)
            }
            Text("▶", color = MaterialTheme.colorScheme.primary, modifier = Modifier.padding(12.dp))
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun RecentGameCard(game: GameInfo, onClick: () -> Unit, onDetails: () -> Unit) {
    Surface(
        modifier = Modifier
            .width(228.dp)
            .height(92.dp)
            .combinedClickable(onClick = onClick, onLongClick = onDetails),
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.surfaceVariant,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.45f)),
    ) {
        Row(Modifier.padding(8.dp), verticalAlignment = Alignment.CenterVertically) {
            GameCover(game, Modifier.width(54.dp).fillMaxHeight())
            Spacer(Modifier.width(11.dp))
            Column(Modifier.weight(1f)) {
                Text(game.title, style = MaterialTheme.typography.titleSmall, maxLines = 2, overflow = TextOverflow.Ellipsis)
                Spacer(Modifier.height(5.dp))
                Text(game.serial ?: game.extension, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}

@Composable
private fun GameMetadata(game: GameInfo) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        StatusChip(game.extension.ifBlank { game.platform.key.uppercase() })
        game.regionFlag?.let { Text(it, fontSize = 14.sp) }
        if (game.compatibility > 0) {
            Text("★".repeat(game.compatibility), color = Color(0xFFFFC857), fontSize = 10.sp, maxLines = 1)
        }
    }
}

@Composable
private fun GameCover(game: GameInfo, modifier: Modifier = Modifier) {
    val context = LocalContext.current
    val custom = remember(game.uri, CustomCovers.version.value) { CustomCovers.fileFor(context, game) }
    val model = custom ?: game.coverUrl
    SubcomposeAsyncImage(
        model = ImageRequest.Builder(context).data(model).crossfade(true).build(),
        contentDescription = game.title,
        modifier = modifier.clip(RoundedCornerShape(12.dp)),
        contentScale = ContentScale.Crop,
        loading = { CoverPlaceholder(game.title) },
        error = { CoverPlaceholder(game.title) },
    )
}

@Composable
private fun CoverPlaceholder(title: String) {
    Box(
        Modifier
            .fillMaxSize()
            .background(
                Brush.linearGradient(
                    listOf(
                        MaterialTheme.colorScheme.primaryContainer,
                        MaterialTheme.colorScheme.surfaceVariant,
                    ),
                ),
            ),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            title.take(2).uppercase(),
            style = MaterialTheme.typography.headlineMedium,
            fontWeight = FontWeight.Black,
            color = MaterialTheme.colorScheme.onPrimaryContainer.copy(alpha = 0.78f),
        )
    }
}

object HomeInputController {
    private var owner: HomeViewModel? = null
    private var openMenu: (() -> Unit)? = null
    private var columns = 5

    fun bind(viewModel: HomeViewModel, onOpenMenu: () -> Unit) {
        owner = viewModel
        openMenu = onOpenMenu
    }

    fun unbind(viewModel: HomeViewModel) {
        if (owner === viewModel) {
            owner = null
            openMenu = null
        }
    }

    fun setColumnCount(value: Int) {
        columns = value.coerceAtLeast(1)
    }

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

    fun scroll(@Suppress("UNUSED_PARAMETER") velocity: Float): Boolean = owner != null

    fun back(): Boolean {
        openMenu?.invoke() ?: return false
        return true
    }
}
