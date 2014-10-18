#include "browsedialog.h"

#include <algorithm>

#include "filelister.h"
#include "gmenu2x.h"
#include "iconbutton.h"
#include "surface.h"
#include "utilities.h"

using std::bind;
using std::max;
using std::min;
using std::string;
using std::tie;
using std::unique_ptr;

BrowseDialog::BrowseDialog(
		GMenu2X *gmenu2x,
		const string &title, const string &subtitle)
	: Dialog(gmenu2x)
	, nameScroll(0)
	, title(title)
	, subtitle(subtitle)
{
}

BrowseDialog::~BrowseDialog()
{
}

void BrowseDialog::initButtonBox()
{
	buttonBox.clear();

	// Cancel to go up a directory, if directories are shown.
	// Accept also goes up a directory if the selection is "..".
	if (fl.getShowDirectories() && getPath() != "/") {
		if (selected < fl.size() && fl[selected] == "..") {
			buttonBox.add(unique_ptr<IconButton>(new IconButton(
					gmenu2x, "skin:imgs/buttons/accept.png")));
		}
		buttonBox.add(unique_ptr<IconButton>(new IconButton(
				gmenu2x, "skin:imgs/buttons/cancel.png",
				gmenu2x->tr["Up one folder"],
				bind(&BrowseDialog::directoryUp, this))));
	}

	// Accept to enter a directory (as opposed to selecting it). -else-
	// Accept to confirm the selection of a file, if files are allowed.
	if (fl[selected] != "..") {
		if (selected < fl.size() && fl.isDirectory(selected)) {
			buttonBox.add(unique_ptr<IconButton>(new IconButton(
					gmenu2x, "skin:imgs/buttons/accept.png",
					gmenu2x->tr["Enter"],
					bind(&BrowseDialog::directoryEnter, this))));
		} else if (canSelect()) {
			buttonBox.add(unique_ptr<IconButton>(new IconButton(
					gmenu2x, "skin:imgs/buttons/accept.png")));
		}
	}

	// Start to confirm the selection of a file or directory if allowed.
	if (canSelect()) {
		buttonBox.add(unique_ptr<IconButton>(new IconButton(
				gmenu2x, "skin:imgs/buttons/start.png",
				gmenu2x->tr["Select"],
				bind(&BrowseDialog::confirm, this))));
	}

	// Cancel (if directories are hidden) or Select to exit.
	if (!fl.getShowDirectories()) {
		buttonBox.add(unique_ptr<IconButton>(new IconButton(
				gmenu2x, "skin:imgs/buttons/cancel.png")));
	}
	buttonBox.add(unique_ptr<IconButton>(new IconButton(
			gmenu2x, "skin:imgs/buttons/select.png",
			gmenu2x->tr["Exit"],
			bind(&BrowseDialog::quit, this))));
}

void BrowseDialog::initIcons()
{
	iconGoUp = gmenu2x->sc.skinRes("imgs/go-up.png");
	iconFolder = gmenu2x->sc.skinRes("imgs/folder.png");
	iconFile = gmenu2x->sc.skinRes("imgs/file.png");
}

void BrowseDialog::initDisplay()
{
	unsigned int top, height;
	tie(top, height) = gmenu2x->getContentArea();

	// Figure out how many items we can fit in the content area.
	rowHeight = gmenu2x->font->getLineSpacing();
	if (fl.getShowDirectories() && iconFolder) {
		rowHeight = max(rowHeight, (unsigned int) (iconFolder->height() + 2));
	}
	numRows = max(height / rowHeight, 1u);
	// Redistribute any leftover space.
	rowHeight = height / numRows;
	topBarHeight = top + (height - rowHeight * numRows) / 2;

	int nameX = fl.getShowDirectories() ? 24 : 5;
	clipRect = (SDL_Rect) {
		static_cast<Sint16>(nameX),
		static_cast<Sint16>(topBarHeight + 1),
		static_cast<Uint16>(gmenu2x->resX - 9 - nameX),
		static_cast<Uint16>(height - 1)
	};
}

void BrowseDialog::centerSelection()
{
	if (fl.size() <= numRows || selected <= numRows / 2) {
		firstElement = 0;
	} else {
		unsigned int lastElement = min(fl.size(), selected + (numRows - numRows / 2));
		firstElement = lastElement - numRows;
	}
}

void BrowseDialog::adjustSelection()
{
	// If the user is moving upwards and the selection would be before the
	// second quarter of the visible rows, or downwards and the selection
	// would be beyond the third quarter of the visible rows, move the
	// viewport if possible. Otherwise leave it where it is.
	const unsigned int bufferRows = numRows / 4;
	if (selected < firstElement + bufferRows) {
		firstElement = max(selected, bufferRows) - bufferRows;
	} else if (selected >= firstElement + numRows - bufferRows) {
		firstElement = min(selected + bufferRows + 1, fl.size()) - numRows;
	}
}

void BrowseDialog::resetNameScroll()
{
	nameScroll = 0;
}

void BrowseDialog::applyNameScroll(bool left)
{
	if (selected < fl.size()) {
		int newNameScroll = (int) nameScroll + (left ? -5 : 5);
		int nameWidth = gmenu2x->font->getTextWidth(fl[selected]);
		newNameScroll = min(newNameScroll, min(nameWidth, 32767) - clipRect.w);
		newNameScroll = max(newNameScroll, 0);
		nameScroll = newNameScroll;
	}
}

void BrowseDialog::initPath()
{
	string path = getPath();
	if (path.empty()) {
		path = CARD_ROOT;
	}
	// fl.browse has to run at least once.
	while (!fl.browse(path) && fl.getShowDirectories() && path != "/") {
		// The given directory could not be opened; try parent.
		path = parentDir(path);
	}
	setPath(path);
}

void BrowseDialog::initSelection()
{
	selected = 0;
}

bool BrowseDialog::exec()
{
	initIcons();
	initDisplay();

	initPath();
	initSelection();
	centerSelection();

	close = false;
	while (!close) {
		initButtonBox();

		paint();

		handleInput();
	}

	return result;
}

BrowseDialog::Action BrowseDialog::getAction(InputManager::Button button)
{
	switch (button) {
		case InputManager::MENU:
			return BrowseDialog::ACT_CLOSE;
		case InputManager::UP:
			return BrowseDialog::ACT_UP;
		case InputManager::DOWN:
			return BrowseDialog::ACT_DOWN;
		case InputManager::LEFT:
			return BrowseDialog::ACT_SCROLLUP;
		case InputManager::RIGHT:
			return BrowseDialog::ACT_SCROLLDOWN;
		case InputManager::ALTLEFT:
			return BrowseDialog::ACT_SCROLLLEFT;
		case InputManager::ALTRIGHT:
			return BrowseDialog::ACT_SCROLLRIGHT;
		case InputManager::CANCEL:
			return BrowseDialog::ACT_GOUP;
		case InputManager::ACCEPT:
			return BrowseDialog::ACT_SELECT;
		case InputManager::SETTINGS:
			return BrowseDialog::ACT_CONFIRM;
		default:
			return BrowseDialog::ACT_NONE;
	}
}

void BrowseDialog::handleInput()
{
	InputManager::Button button = gmenu2x->input.waitForPressedButton();
	BrowseDialog::Action action = getAction(button);

	if (action == BrowseDialog::ACT_SELECT
	 && selected < fl.size() && fl[selected] == "..") {
		action = BrowseDialog::ACT_GOUP;
	}

	switch (action) {
	case BrowseDialog::ACT_UP:
		if (fl.size() > 0) {
			selected = (selected == 0)
					? fl.size() - 1
					: selected - 1;
			adjustSelection();
			resetNameScroll();
		}
		break;
	case BrowseDialog::ACT_SCROLLUP:
		if (fl.size() > 0) {
			selected = (selected <= numRows - 2)
					? 0
					: selected - (numRows - 2);
			adjustSelection();
			resetNameScroll();
		}
		break;
	case BrowseDialog::ACT_DOWN:
		if (fl.size() > 0) {
			selected = (fl.size() - 1 <= selected)
					? 0
					: selected + 1;
			adjustSelection();
			resetNameScroll();
		}
		break;
	case BrowseDialog::ACT_SCROLLDOWN:
		if (fl.size() > 0) {
			selected = (selected + (numRows - 2) >= fl.size())
					? fl.size() - 1
					: selected + (numRows - 2);
			adjustSelection();
			resetNameScroll();
		}
		break;
	case BrowseDialog::ACT_SCROLLLEFT:
		applyNameScroll(true);
		break;
	case BrowseDialog::ACT_SCROLLRIGHT:
		applyNameScroll(false);
		break;
	case BrowseDialog::ACT_GOUP:
		if (fl.getShowDirectories()) {
			directoryUp();
			break;
		}
		/* Fall through (If not showing directories, GOUP acts as CLOSE) */
	case BrowseDialog::ACT_CLOSE:
		quit();
		break;
	case BrowseDialog::ACT_SELECT:
		if (selected < fl.size() && fl.isDirectory(selected)) {
			directoryEnter();
			break;
		}
		/* Fall through (If not a directory, SELECT acts as CONFIRM) */
	case BrowseDialog::ACT_CONFIRM:
		if (selected < fl.size() && canSelect()) {
			confirm();
		}
		break;
	default:
		break;
	}
}

void BrowseDialog::directoryUp()
{
	string oldDir = getPath();
	if (oldDir != "/") {
		string newDir = parentDir(oldDir);
		setPath(newDir);
		fl.browse(newDir);
		// Find the position of the previous directory among the directories of
		// the parent. If it's not found, select 0.
		string oldName = oldDir.substr(newDir.size(), oldDir.size() - newDir.size() - 1);
		auto& subdirs = fl.getDirectories();

		auto it = find(subdirs.begin(), subdirs.end(), oldName);
		selected = it == subdirs.end() ? 0 : it - subdirs.begin();
		centerSelection();
		resetNameScroll();
	}
}

void BrowseDialog::directoryEnter()
{
	string newDir = getPath() + fl[selected] + "/";
	setPath(newDir);

	fl.browse(newDir);
	selected = 0;
	adjustSelection();
	resetNameScroll();
}

void BrowseDialog::confirm()
{
	result = true;
	close = true;
}

void BrowseDialog::quit()
{
	result = false;
	close = true;
}

void BrowseDialog::paintBackground()
{
	gmenu2x->bg->blit(*gmenu2x->s, 0, 0);
}

void BrowseDialog::paintIcon()
{
	drawTitleIcon(*gmenu2x->s, "icons/explorer.png", true);
}

void BrowseDialog::paint()
{
	OutputSurface& s = *gmenu2x->s;

	unsigned int i, iY;
	unsigned int offsetY;

	paintBackground();
	paintIcon();
	writeTitle(*gmenu2x->s, title);
	writeSubTitle(*gmenu2x->s, subtitle);
	buttonBox.paint(*gmenu2x->s, 5, gmenu2x->resY - 1);

	unsigned int lastElement = firstElement + numRows;
	if (lastElement > fl.size()) {
		lastElement = fl.size();
		firstElement = max(lastElement, numRows) - numRows;
	}

	offsetY = topBarHeight + 1;

	//Files & Directories
	if (fl.size() == 0) {
		gmenu2x->font->write(s, "(" + gmenu2x->tr["no items"] + ")",
				4, topBarHeight + rowHeight / 2,
				Font::HAlignLeft, Font::VAlignMiddle);
	} else {
		//Selection
		iY = topBarHeight + 1 + (selected - firstElement) * rowHeight;
		s.box(2, iY, clipRect.x + clipRect.w - 2, rowHeight - 1,
				gmenu2x->skinConfColors[COLOR_SELECTION_BG]);

		for (i = firstElement; i < lastElement; i++) {
			if (fl.getShowDirectories()) {
				Surface *icon;
				if (fl.isDirectory(i)) {
					if (fl[i] == "..") {
						icon = iconGoUp;
					} else {
						icon = iconFolder;
					}
				} else {
					icon = iconFile;
				}
				if (icon) {
					icon->blit(s, 5, offsetY);
				}
			}
			s.setClipRect(clipRect);
			gmenu2x->font->write(s, sanitizeFileName(fl[i]),
					clipRect.x - (i == selected ? nameScroll : 0),
					offsetY + rowHeight / 2,
					Font::HAlignLeft, Font::VAlignMiddle);
			s.clearClipRect();

			offsetY += rowHeight;
		}
	}

	gmenu2x->drawScrollBar(numRows, fl.size(), firstElement);
	s.flip();
}

string const& BrowseDialog::getPath() {
	return path;
}

void BrowseDialog::setPath(std::string const& path)
{
	this->path = path;
	if (!path.empty() && path.back() != '/') {
		this->path.push_back('/');
	}
}

string BrowseDialog::getFile() {
	return fl[selected];
}

bool BrowseDialog::canSelect() {
	return selected < fl.size() && fl.isFile(selected);
}
