.pragma library

// Single source of truth for colours, radii, spacing and type sizes.
// QML must not hard-code hex values anywhere else: changing the palette here
// changes the whole application.

// --- brand -----------------------------------------------------------------
var green = "#188568"          // primary action, active navigation
var greenDeep = "#14634a"      // headline text on the tinted hero card
var greenPressed = "#12674f"
var greenHover = "#219777"
var greenSoft = "#eaf4ee"      // active navigation pill
var greenTint = "#edf6f0"      // highlighted panel
var greenTintStrong = "#e6f5ed" // hero card when the proxy runs
var greenEdge = "#d9e9df"      // border of a tinted panel
var greenLine = "#71b397"      // border of a selected application card
var greenWash = "#eff8f1"      // background of a selected application card
var greenIcon = "#699782"

// --- text ------------------------------------------------------------------
var ink = "#202c30"
var inkSoft = "#6e7d76"
var muted = "#889497"
var mutedWarm = "#96aa9d"
var mutedFaint = "#a4b0a9"
var mutedLine = "#bac7bf"
var subtitle = "#81988a"
var placeholder = "#9aa5a5"

// --- surfaces --------------------------------------------------------------
var surface = "#f5f8f7"        // window background
var chrome = "#fdfefd"         // sidebar
var card = "#ffffff"
var cardSoft = "#fbfcfb"
var cardTint = "#f5f8f5"       // sidebar status block
var field = "#f8faf9"
var hover = "#edf3f1"
var hoverSoft = "#f7faf8"
var hoverTint = "#eaf2ed"
var line = "#e7edeb"
var lineSoft = "#ebf0ec"
var edge = "#dce4e0"           // window frame border
var focus = "#75b9a2"
var iconPlate = "#f0f5f2"
var iconFallback = "#e1eee7"
var chipActive = "#e2f0e6"
var chipActiveEdge = "#c6dfcf"
var chipHover = "#edf3ef"
var chipText = "#83968a"
var pillOn = "#e3f3eb"
var pillOff = "#eaf0ed"
var pillDot = "#99a89e"
var pillText = "#7e8f83"

// --- states ----------------------------------------------------------------
var warn = "#aa8359"
var warnSoft = "#f8eee8"
var warnText = "#c28c65"
var danger = "#c9675a"
var dangerText = "#b65a4e"
var dangerSoft = "#fbecea"
var dangerHover = "#fce9e8"
var goodText = "#3a9068"
var goodSoft = "#e8f3ec"
var neutralSoft = "#eff2f0"
var tipBackground = "#263b31"
var tipText = "#eef5f0"
var windowButton = "#e9efec"
var windowButtonClose = "#fce9e8"
var captionIcon = "#8a9790"
var captionClose = "#c95a54"
var overlay = "#e6f5f8f6"

// --- geometry --------------------------------------------------------------
var radiusWindow = 16
var radiusCard = 16
var radiusRow = 12
var radiusIcon = 10
var radiusControl = 10
var radiusChip = 8
var radiusPill = 14

var sidebarWidth = 214
var contentLeft = 240
var contentRight = 28
var contentTop = 56
var contentBottom = 56
var actionHeight = 42
var fieldHeight = 46
var chipHeight = 30

// --- type ------------------------------------------------------------------
var fontFamily = "Microsoft YaHei UI"
var fontSize = 13
var pageTitleSize = 24
var pageSubtitleSize = 12
var sectionTitleSize = 15
var captionSize = 11
var microSize = 10

// --- component-level detail colours ----------------------------------------
var navHover = "#f2f6f3"
var navIcon = "#8b9892"
var navFocus = "#a5cbb5"
var statusDot = "#b4c1b8"
var heroGlyph = "#86a895"
var heroSubtitle = "#8b9c90"
var powerRing = "#e4eee5"
var powerFill = "#eef5ed"
var powerButton = "#35483f"
var powerButtonHover = "#475c53"
var powerActive = "#168568"
var cardEdge = "#e9eeea"
var iconPlateEdge = "#edf1ed"
var iconGlyph = "#91ad9e"
var checkEdge = "#d3ded6"
var dotRunning = "#81ae93"
var dotIdle = "#b7c1ba"
var missingText = "#bd8c68"
var morePressed = "#dcebe2"
var comboHover = "#f4f8f5"
var comboGlyph = "#81968a"
var nodeActive = "#edf7f0"
var nodeHover = "#f6faf7"
var nodeActiveEdge = "#8ebda3"
var nodeIconActive = "#d4ecdd"
var nodeIcon = "#f0f4f1"
var meterTrack = "#e8eeea"
var meterHigh = "#d49368"
var meterFill = "#70b899"
var cardGlyph = "#8ba899"
var footerDot = "#a3b7a8"
var footerText = "#9aaba0"
var nodeGlyph = "#789b86"
var emptyGlyph = "#8bae99"
