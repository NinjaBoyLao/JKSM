#include <3ds.h>
#include <citro2d.h>
#include <string>
#include <vector>

#include <stdio.h>

#include "data.h"
#include "gfx.h"
#include "ui.h"
#include "util.h"
#include "fs.h"
#include "sys.h"

static ui::menu mainMenu, titleMenu, backupMenu, nandMenu, nandBackupMenu, folderMenu;

enum states
{
	MAIN_MENU,
	TITLE_MENU,
	BACK_MENU,
	SYS_MENU,
	SYS_BAKMENU,
	FLDR_MENU
};

//Prev is only needed to flip back to the correct backup menu
static uint16_t state = MAIN_MENU, prev = MAIN_MENU;

namespace ui
{
	void loadTitleMenu()
	{
		titleMenu.reset();
		for(unsigned i = 0; i < data::titles.size(); i++)
			titleMenu.addOpt(util::toUtf8(data::titles[i].getTitle()));
	}

	void loadNandMenu()
	{
		nandMenu.reset();
		for(unsigned i = 0; i < data::nand.size(); i++)
			nandMenu.addOpt(util::toUtf8(data::nand[i].getTitle()));
	}

	void prepMenus()
	{
		//Main
		mainMenu.addOpt("Titles");
		mainMenu.addOpt("System Titles");
		mainMenu.addOpt("Reload Titles");
		mainMenu.addOpt("Play Coins");
		mainMenu.addOpt("Exit");

		//Title menu
		loadTitleMenu();

		loadNandMenu();

		backupMenu.addOpt("Save Data");
		backupMenu.addOpt("Delete Save Data");
		backupMenu.addOpt("Extra Data");
		backupMenu.addOpt("Delete Extra Data");
		backupMenu.addOpt("Back");

		nandBackupMenu.addOpt("System Save");
		nandBackupMenu.addOpt("Extra Data");
		nandBackupMenu.addOpt("BOSS Extra Data");
		nandBackupMenu.addOpt("Back");
	}

	void drawTopBar(const std::string& info)
	{
		C2D_DrawRectSolid(0, 0, 0.5f, 400, 16, 0xFF505050);
		C2D_DrawRectSolid(0, 17, 0.5f, 400, 1, 0xFF1D1D1D);
		gfx::drawText(info, 4, 0, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF));
	}

	void stateMainMenu(const uint32_t& down, const uint32_t& held)
	{
		mainMenu.handleInput(down, held);

		if(down & KEY_A)
		{
			switch(mainMenu.getSelected())
			{
				case 0:
					state = TITLE_MENU;
					break;

				case 1:
					state = SYS_MENU;
					break;

				case 2:
					remove("/JKSV/titles");
					data::loadTitles();
					loadTitleMenu();
					break;

				case 3:
					util::setPC();
					break;

				case 4:
					sys::run = false;
					break;
			}
		}

		gfx::frameBegin();
		gfx::frameStartTop();
		drawTopBar("JKSM - 7/12/2018");
		mainMenu.draw(40, 82, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF), 320);
		gfx::frameStartBot();
		gfx::frameEnd();
	}

	void stateTitleMenu(const uint32_t& down, const uint32_t& held)
	{
		//Much needed Jump button
		static ui::button jumpTo("Jump To", 0, 208, 320, 32);

		data::cartCheck();

		titleMenu.handleInput(down, held);

		touchPosition p;
		hidTouchRead(&p);

		jumpTo.update(p);

		if(down & KEY_A)
		{
			data::curData = data::titles[titleMenu.getSelected()];

			state = BACK_MENU;
		}
		else if(down & KEY_B)
		{
			state = MAIN_MENU;
		}
		else if(jumpTo.getEvent() == BUTTON_RELEASED)
		{
			std::string getChar = util::toUtf8(util::getString("Enter a letter to jump to."));
			if(!getChar.empty())
			{
				//Only use first char
				char jmpTo = std::tolower(getChar[0]);

				//Skip cart
				for(unsigned i = 1; i < titleMenu.getCount(); i++)
				{
					if(std::tolower(titleMenu.getOpt(i)[0]) == jmpTo)
					{
						titleMenu.setSelected(i);
						break;
					}
				}
			}
		}

		gfx::frameBegin();
		gfx::frameStartTop();
		drawTopBar("Select a Title");
		titleMenu.draw(40, 34, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF), 320);
		gfx::frameStartBot();
		data::titles[titleMenu.getSelected()].drawInfo(8, 8);
		jumpTo.draw();
		gfx::frameEnd();
	}

	void prepFolderMenu(const data::titleData& dat, const uint32_t& mode)
	{
		std::u16string path = util::createPath(dat, mode);

		fs::dirList bakDir(fs::getSDMCArch(), path);

		folderMenu.reset();
		folderMenu.addOpt("New");

		for(unsigned i = 0; i < bakDir.getCount(); i++)
			folderMenu.addOpt(util::toUtf8(bakDir.getItem(i)));
	}

	void stateBackupMenu(const uint32_t& down, const uint32_t& held)
	{
		backupMenu.handleInput(down, held);

		if(down & KEY_A)
		{
			switch(backupMenu.getSelected())
			{
				case 0:
					if(fs::openArchive(data::curData, ARCHIVE_USER_SAVEDATA))
					{
						util::createTitleDir(data::curData, ARCHIVE_USER_SAVEDATA);
						prepFolderMenu(data::curData, ARCHIVE_USER_SAVEDATA);
						prev  = BACK_MENU;
						state = FLDR_MENU;
					}
					break;

				case 1:
					if(confirm("Are you sure you want to delete all data for this game?") && fs::openArchive(data::curData, ARCHIVE_USER_SAVEDATA))
					{
						FSUSER_DeleteDirectoryRecursively(fs::getSaveArch(), fsMakePath(PATH_ASCII, "/"));
						fs::commitData(ARCHIVE_USER_SAVEDATA);
						FSUSER_CloseArchive(fs::getSaveArch());
					}
					break;

				case 2:
					if(fs::openArchive(data::curData, ARCHIVE_EXTDATA))
					{
						util::createTitleDir(data::curData, ARCHIVE_EXTDATA);
						prepFolderMenu(data::curData, ARCHIVE_EXTDATA);
						prev  = BACK_MENU;
						state = FLDR_MENU;
					}
					break;

				case 3:
					{
						std::string confStr = "Are you 100% sure you want to delete the Extra Data for \"" + util::toUtf8(data::curData.getTitle()) + "\"?";
						if(confirm(confStr))
						{
							FS_ExtSaveDataInfo del = { MEDIATYPE_SD, 0, 0, data::curData.getExtData(), 0 };

							Result res = FSUSER_DeleteExtSaveData(del);
							if(R_SUCCEEDED(res))
								showMessage("Extdata deleted!");
						}
					}
					break;

				case 4:
					state = TITLE_MENU;
					break;
			}
		}
		else if(down & KEY_B)
		{
			state = TITLE_MENU;
		}

		gfx::frameBegin();
		gfx::frameStartTop();
		drawTopBar(util::toUtf8(data::curData.getTitleWide()));
		backupMenu.draw(40, 82, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF), 320);
		gfx::frameStartBot();
		gfx::frameEnd();
	}

	void stateNandMenu(const uint32_t& down, const uint32_t& held)
	{
		nandMenu.handleInput(down, held);

		if(down & KEY_A)
		{
			data::curData = data::nand[nandMenu.getSelected()];
			state = SYS_BAKMENU;
		}
		else if(down & KEY_B)
			state = MAIN_MENU;

		gfx::frameBegin();
		gfx::frameStartTop();
		drawTopBar("Select a NAND Title");
		nandMenu.draw(40, 34, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF), 320);
		gfx::frameStartBot();
		data::nand[nandMenu.getSelected()].drawInfo(8, 8);
		gfx::frameEnd();
	}

	void stateNandBack(const uint32_t& down, const uint32_t& held)
	{
		nandBackupMenu.handleInput(down, held);

		if(down & KEY_A)
		{
			switch(nandBackupMenu.getSelected())
			{
				case 0:
					if(fs::openArchive(data::curData, ARCHIVE_SYSTEM_SAVEDATA))
					{
						util::createTitleDir(data::curData, ARCHIVE_SYSTEM_SAVEDATA);
						prepFolderMenu(data::curData, ARCHIVE_SYSTEM_SAVEDATA);
						prev  = SYS_BAKMENU;
						state = FLDR_MENU;
					}
					break;

				case 1:
					if(fs::openArchive(data::curData, ARCHIVE_EXTDATA))
					{
						util::createTitleDir(data::curData, ARCHIVE_EXTDATA);
						prepFolderMenu(data::curData, ARCHIVE_EXTDATA);
						prev  = SYS_BAKMENU;
						state = FLDR_MENU;
					}
					break;

				case 2:
					if(fs::openArchive(data::curData, ARCHIVE_BOSS_EXTDATA))
					{
						util::createTitleDir(data::curData, ARCHIVE_BOSS_EXTDATA);
						prepFolderMenu(data::curData, ARCHIVE_BOSS_EXTDATA);
						prev  = SYS_BAKMENU;
						state = FLDR_MENU;
					}
					break;

				case 3:
					state = SYS_MENU;
					break;
			}
		}
		else if(down & KEY_B)
			state = SYS_MENU;

		gfx::frameBegin();
		gfx::frameStartTop();
		drawTopBar(util::toUtf8(data::curData.getTitleWide()));
		nandBackupMenu.draw(40, 88, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF), 320);
		gfx::frameStartBot();
		gfx::frameEnd();
	}

	void stateFolderMenu(const uint32_t& down, const uint32_t& held)
	{
		folderMenu.handleInput(down, held);

		int sel = folderMenu.getSelected();

		if(down & KEY_A)
		{
			//New
			if(sel == 0)
			{
				std::u16string newFolder = util::getString("Enter a new folder name");
				if(!newFolder.empty())
				{
					std::u16string fullPath = util::createPath(data::curData, fs::getSaveMode()) + newFolder;
					FSUSER_CreateDirectory(fs::getSDMCArch(), fsMakePath(PATH_UTF16, fullPath.data()), 0);
					fullPath += util::toUtf16("/");

					fs::backupArchive(fullPath);

					prepFolderMenu(data::curData, fs::getSaveMode());
				}
			}
			else
			{
				sel--;

				fs::dirList titleDir(fs::getSDMCArch(), util::createPath(data::curData, fs::getSaveMode()));
				std::u16string fullPath = util::createPath(data::curData, fs::getSaveMode()) + titleDir.getItem(sel);

				//Del
				FSUSER_DeleteDirectoryRecursively(fs::getSDMCArch(), fsMakePath(PATH_UTF16, fullPath.data()));

				//Recreate
				FSUSER_CreateDirectory(fs::getSDMCArch(), fsMakePath(PATH_UTF16, fullPath.data()), 0);

				fullPath += util::toUtf16("/");
				fs::backupArchive(fullPath);
			}
		}
		else if(down &  KEY_Y && sel != 0)
		{
			sel--;
			fs::dirList titleDir(fs::getSDMCArch(), util::createPath(data::curData, fs::getSaveMode()));
			std::string confStr = "Are you sure you want to restore \"" + util::toUtf8(titleDir.getItem(sel)) + "\"?";
			if(confirm(confStr))
			{
				std::u16string restPath = util::createPath(data::curData, fs::getSaveMode()) + titleDir.getItem(sel) + util::toUtf16("/");

				//Wipe root
				FSUSER_DeleteDirectoryRecursively(fs::getSaveArch(), fsMakePath(PATH_ASCII, "/"));

				//Restore from restPath
				fs::restoreToArchive(restPath);
			}
		}
		else if(down & KEY_X && sel != 0)
		{
			sel--;
			fs::dirList titleDir(fs::getSDMCArch(), util::createPath(data::curData, fs::getSaveMode()));
			std::string confStr = "Are you sure you want to delete \"" + util::toUtf8(titleDir.getItem(sel)) + "\"?";
			if(confirm(confStr))
			{
				std::u16string delPath = util::createPath(data::curData, fs::getSaveMode()) + titleDir.getItem(sel);

				FSUSER_DeleteDirectoryRecursively(fs::getSDMCArch(), fsMakePath(PATH_UTF16, delPath.data()));

				prepFolderMenu(data::curData, fs::getSaveMode());
			}
		}
		else if(down & KEY_B)
			state = prev;

		gfx::frameBegin();
		gfx::frameStartTop();
		drawTopBar("Select a Folder");
		folderMenu.draw(40, 34, 0xFFFFFFFF, 320);
		gfx::frameStartBot();
		gfx::drawText("A = Select\nY = Restore\nX = Delete\nB = Back", 16, 16, 0xFFFFFFFF);
		gfx::frameEnd();

	}

	void runApp(const uint32_t& down, const uint32_t& held)
	{
		switch(state)
		{
			case MAIN_MENU:
				stateMainMenu(down, held);
				break;

			case TITLE_MENU:
				stateTitleMenu(down, held);
				break;

			case BACK_MENU:
				stateBackupMenu(down, held);
				break;

			case SYS_MENU:
				stateNandMenu(down, held);
				break;

			case SYS_BAKMENU:
				stateNandBack(down, held);
				break;

			case FLDR_MENU:
				stateFolderMenu(down, held);
				break;
		}
	}

	void advMode(const FS_Archive& arch)
	{
		std::u16string svPath = util::toUtf16("/"), sdPath = util::toUtf16("/");
		ui::menu svMenu, sdMenu, copyMenu;
		fs::dirList svList(arch, svPath), sdList(fs::getSDMCArch(), sdPath);

		util::copyDirlistToMenu(svList, svMenu);
		util::copyDirlistToMenu(sdList, sdMenu);

		copyMenu.addOpt("Copy");
		copyMenu.addOpt("Delete");
		copyMenu.addOpt("Rename");
		copyMenu.addOpt("Make Dir");
		copyMenu.addOpt("Back");

		int cMenu = 0, pMenu = 0;

		while(true)
		{
			hidScanInput();

			uint32_t down = hidKeysDown();
			uint32_t held = hidKeysHeld();

			if(down & KEY_A)
			{
				switch(cMenu)
				{
					//save
					case 0:
						{
							if(svMenu.getSelected() > 1)
							{
								int sel = svMenu.getSelected() - 2;
								if(svList.isDir(sel))
								{
									svPath += svList.getItem(sel) + util::toUtf16("/");
									svList.reassign(svPath);
									svMenu.reset();
									util::copyDirlistToMenu(svList, svMenu);
								}
							}
						}
						break;

					//sd
					case 1:
						{
							if(sdMenu.getSelected() > 1)
							{
								int sel = sdMenu.getSelected() - 2;
								if(sdList.isDir(sel))
								{
									sdPath += sdList.getItem(sel) + util::toUtf16("/");
									sdList.reassign(sdPath);
									sdMenu.reset();
									util::copyDirlistToMenu(sdList, sdMenu);
								}
							}
						}
						break;
				}
			}
			else if(down & KEY_B)
			{
				switch(cMenu)
				{
					case 0:
						if(svPath != util::toUtf16("/"))
						{
							util::removeLastDirFromString(svPath);
							svList.reassign(svPath);
							svMenu.reset();
							util::copyDirlistToMenu(svList, svMenu);
						}
						break;

					case 1:
						if(sdPath != util::toUtf16("/"))
						{
							util::removeLastDirFromString(sdPath);
							sdList.reassign(sdPath);
							sdMenu.reset();
							util::copyDirlistToMenu(sdList, sdMenu);
						}
						break;
				}
			}
			else if(down & KEY_X)
			{
				if(cMenu == 2)
					cMenu = pMenu;
				else
				{
					pMenu = cMenu;
					cMenu = 2;
				}
			}
			else if(down & KEY_R || down & KEY_L)
			{
				if(cMenu == 0)
					cMenu = 1;
				else
					cMenu = 0;
			}
			else if(down & KEY_SELECT)
				break;

			switch(cMenu)
			{
				//save
				case 0:
					svMenu.handleInput(down, held);
					break;

				//sd
				case 1:
					sdMenu.handleInput(down, held);
					break;

				//copy
				case 2:
					copyMenu.handleInput(down, held);
					break;
			}

			gfx::frameBegin();
			gfx::frameStartTop();
			ui::drawTopBar("Adv. Mode");
			gfx::drawU16Text(util::toUtf16("sv:") + svPath, 0, 16, 0xFFFFFFFF);
			svMenu.draw(40, 32, 0xFFFFFFFF, 320);
			if(cMenu == 2 && pMenu == 0)
			{
				copyMenu.draw(120, 90, 0xFFFFFFFF, 160);
			}

			gfx::frameStartBot();
			gfx::drawU16Text(util::toUtf16("sd:") + sdPath, 0, 0, 0xFFFFFFFF);
			sdMenu.draw(0, 32, 0xFFFFFFFF, 320);
			if(cMenu == 2 && pMenu == 1)
			{
				copyMenu.draw(80, 90, 0xFFFFFFFF, 160);
			}
			gfx::frameEnd();
		}
	}

	void showMessage(const std::string& mess)
	{
		while(1)
		{
			hidScanInput();

			uint32_t down = hidKeysDown();

			if(down & KEY_A)
				break;

			gfx::frameBegin();
			gfx::frameStartBot();
			C2D_DrawRectSolid(8, 8, 0.5f, 304, 224, C2D_Color32(231, 231, 231, 0xFF));
			gfx::drawText(mess, 16, 16, C2D_Color32(0, 0, 0, 0xFF));
			gfx::frameEnd();
		}
	}

	void menu::addOpt(const std::string& add)
	{
		opt.push_back(add);
	}

	void menu::reset()
	{
		opt.clear();

		selected = 0;
		start = 0;
	}

	void menu::setSelected(const int& newSel)
	{
		if(newSel < start || newSel > start + 15)
		{
			int size = opt.size() - 1;
			if(newSel + 15 > size)
				start = size - 14;
			else
				start = newSel;

			selected = newSel;
		}
		else
			selected = newSel;
	}

	void menu::handleInput(const uint32_t& key, const uint32_t& held)
	{
		if( (held & KEY_UP) || (held & KEY_DOWN))
			fc++;
		else
			fc = 0;
		if(fc > 10)
			fc = 0;

		int size = opt.size() - 1;
		if((key & KEY_UP) || ((held & KEY_UP) && fc == 10))
		{
			selected--;
			if(selected < 0)
				selected = size;

			if((start > selected)  && (start > 0))
				start--;
			if(size < 15)
				start = 0;
			if(selected == size && size > 15)
				start = size - 14;
		}
		else if((key & KEY_DOWN) || ((held & KEY_DOWN) && fc == 10))
		{
			selected++;
			if(selected > size)
				selected = 0;

			if((selected > (start + 14)) && ((start + 14) < size))
				start++;
			if(selected == 0)
				start = 0;
		}
		else if(key & KEY_RIGHT)
		{
			selected += 7;
			if(selected > size)
				selected = size;
			if((selected - 14) > start)
				start = selected - 14;
		}
		else if(key & KEY_LEFT)
		{
			selected -= 7;
			if(selected < 0)
				selected = 0;
			if(selected < start)
				start = selected;
		}
	}

	void menu::draw(const int& x, const int& y, const uint32_t& baseClr, const uint32_t& rectWidth)
	{
		if(clrAdd)
		{
			clrSh += 4;
			if(clrSh > 63)
				clrAdd = false;
		}
		else
		{
			clrSh--;
			if(clrSh == 0)
				clrAdd = true;
		}

		int length = 0;
		if((opt.size() - 1) < 15)
			length = opt.size();
		else
			length = start + 15;

		uint32_t rectClr = 0xFF << 24 | ((0xBB + clrSh) & 0xFF) << 16 | ((0x88 + clrSh) << 8) | 0x00;

		for(int i = start; i < length; i++)
		{
			if(i == selected)
				C2D_DrawRectSolid(x, (y + 2) + ((i - start) * 12), 0.5f, rectWidth, 12, rectClr);

			gfx::drawText(opt[i], x, y + ((i - start) * 12), 0xFFFFFFFF);
		}
	}

	progressBar::progressBar(const uint32_t& _max)
	{
		max = (float)_max;
	}

	void progressBar::update(const uint32_t& _prog)
	{
		prog = (float)_prog;

		float percent = (float)(prog / max) * 100;
		width  = (float)(percent * 288) / 100;
	}

	void progressBar::draw(const std::string& text)
	{
		C2D_DrawRectSolid(8, 8, 0.5f, 304, 224, C2D_Color32(231, 231, 231, 0xFF));
		C2D_DrawRectSolid(16, 200, 0.5f, 288, 16, C2D_Color32(0, 0, 0, 0xFF));
		C2D_DrawRectSolid(16, 200, 0.5f, width, 16, C2D_Color32(0, 0xFF, 0, 0xFF));
		gfx::drawText(text, 16, 16, C2D_Color32(0, 0, 0, 0xFF));
	}

	bool confirm(const std::string& mess)
	{
		std::string wrapped = util::getWrappedString(mess, 288);

		button yes("Yes (A)", 16, 192, 128, 32);
		button no("No (B)", 176, 192, 128, 32);

		while(true)
		{
			hidScanInput();

			uint32_t down = hidKeysDown();
			touchPosition p;
			hidTouchRead(&p);

			if(down & KEY_A || yes.getEvent() == BUTTON_RELEASED)
				return true;
			else if(down & KEY_B || no.getEvent() == BUTTON_RELEASED)
				return false;

			gfx::frameBegin();
			gfx::frameStartBot();
			C2D_DrawRectSolid(8, 8, 0.5f, 304, 224, 0xFFF4F4F4);
			gfx::drawText(wrapped, 16, 16, 0xFF000000);
			yes.draw();
			no.draw();
			gfx::frameEnd();
		}
		return false;
	}
}