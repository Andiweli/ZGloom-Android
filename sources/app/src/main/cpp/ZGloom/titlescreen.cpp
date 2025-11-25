#include "titlescreen.h"
#include "SaveSystem.h"
extern bool g_RequestTitleContinue;


void TitleScreen::Render(SDL_Surface* src, SDL_Surface* dest, Font& font)
{
	SDL_BlitSurface(src, nullptr, dest, nullptr);
	bool flash = (timer / 5) & 1;

	if (status == TITLESTATUS_MAIN)
	{
		bool hasSave = SaveSystem::HasSave();

		// Ohne Save nie auf dem unsichtbaren RESUME-Eintrag stehen bleiben
		if (!hasSave && selection == MAINENTRY_RESUME)
			selection = MAINENTRY_PLAY;

		if (hasSave)
		{
			// With save: show RESUME above START NEW GAME
			if (flash || (selection != MAINENTRY_RESUME)) font.PrintMessage("RESUME SAVED POSITION", 150, dest, 1);
			if (flash || (selection != MAINENTRY_PLAY))   font.PrintMessage("START NEW GAME", 165, dest, 1);
			if (flash || (selection != MAINENTRY_SELECT)) font.PrintMessage("LEVEL SELECT", 180, dest, 1);
			if (flash || (selection != MAINENTRY_ABOUT))  font.PrintMessage("ABOUT GLOOM", 195, dest, 1);
			if (flash || (selection != MAINENTRY_QUIT))   font.PrintMessage("EXIT GAME", 210, dest, 1);
		}
		else
		{
			// Without save: classic 4-entry menu
			if (flash || (selection != MAINENTRY_PLAY))   font.PrintMessage("START NEW GAME", 160, dest, 1);
			if (flash || (selection != MAINENTRY_SELECT)) font.PrintMessage("LEVEL SELECT", 175, dest, 1);
			if (flash || (selection != MAINENTRY_ABOUT))  font.PrintMessage("ABOUT GLOOM", 190, dest, 1);
			if (flash || (selection != MAINENTRY_QUIT))   font.PrintMessage("EXIT GAME", 205, dest, 1);
		}

		font.PrintMessage("ZGLOOM ENGINE OUYA 11.2025", 243, dest, 1);
	}
	else if (status == TITLESTATUS_SELECT)
	{
		for (int i = selection - 10; i < selection + 10; i++)
		{
			if ((i >= 0) && (i < (int)levelnames.size()))
			{
				if (flash || (i!=selection))
				{
					std::string name = levelnames[i];
					const std::size_t kMaxChars = 36;
					if (name.length() > kMaxChars)
					{
						name = name.substr(0, kMaxChars);
						name += "...";
					}
					font.PrintMessage(name.c_str(), 100+(i-selection)*10, dest, 1);
				}
			}
		}
	}
	else
	{
		font.PrintMessage("GLOOM ENGINE", 30, dest, 1);
		font.PrintMessage("BY BLACK MAGIC", 40, dest, 1);

		font.PrintMessage("PROGRAMMED BY MARK SIBLY", 55, dest, 1);
		font.PrintMessage("GRAPHICS BY THE BUTLER BROTHERS", 65, dest, 1);
		font.PrintMessage("MUSIC BY KEV STANNARD", 75, dest, 1);
		font.PrintMessage("AUDIO BY BLACK MAGIC", 85, dest, 1);
		
		font.PrintMessage("CODED IN DEVPAC2", 100, dest, 1);
		font.PrintMessage("UTILITIES CODED IN BLITZ BASIC 2", 110, dest, 1);
		font.PrintMessage("RENDERED IN DPAINT3 AND DPAINT4", 120, dest, 1);
		font.PrintMessage("DECRUNCHCODE BY THOMAS SCHWARZ", 130, dest, 1);

		font.PrintMessage("GLOOM3 AND ZOMBIE MASSACRE", 145, dest, 1);
		font.PrintMessage("BY ALPHA SOFTWARE", 155, dest, 1);

		font.PrintMessage("ABOUT THIS PORT", 175, dest, 1);
		font.PrintMessage("CODE AND ADDITIONAL EXTRAS", 190, dest, 1);
		font.PrintMessage("BY ANDIWELI", 200, dest, 1);
		font.PrintMessage("BASED ON X86 PORT", 215, dest, 1);
	}
}

TitleScreen::TitleScreen()
{
	status = TITLESTATUS_MAIN;
	selection = 0;
	timer = 0;
}

TitleScreen::TitleReturn TitleScreen::Update(SDL_Event& tevent, int& levelout)
{
	if (tevent.type == SDL_KEYDOWN)
	{
		if (status == TITLESTATUS_MAIN)
		{
			bool hasSave = SaveSystem::HasSave();

			// If there is no save, never stay on the RESUME entry
			if (!hasSave && selection == MAINENTRY_RESUME)
				selection = MAINENTRY_PLAY;

			switch (tevent.key.keysym.sym)
			{
			// nav up and down and vice versa 
	    	case SDLK_DOWN:
	        	if (hasSave)
	        	{
	        		selection++;
	        		if (selection >= MAINENTRY_END)
	        			selection = 0;                        // wrap from bottom to top
	        	}
	        	else
	        	{
	        		selection++;
	        		if (selection > MAINENTRY_QUIT)
	        			selection = MAINENTRY_PLAY;          // wrap within PLAY..QUIT
	        	}
	        	break;

	    	case SDLK_UP:
	        	if (hasSave)
	        	{
	        		selection--;
	        		if (selection < 0)
	        			selection = MAINENTRY_END - 1;       // wrap from top to bottom
	        	}
	        	else
	        	{
	        		selection--;
	        		if (selection < MAINENTRY_PLAY)
	        			selection = MAINENTRY_QUIT;          // wrap within PLAY..QUIT
	        	}
	        	break;

			case SDLK_SPACE:
			case SDLK_RETURN:
			case SDLK_LCTRL:
				if (hasSave && selection == MAINENTRY_RESUME)
				{
					g_RequestTitleContinue = true;
					return TITLERET_PLAY;
				}
				if (selection == MAINENTRY_PLAY)   return TITLERET_PLAY;
				if (selection == MAINENTRY_QUIT)   return TITLERET_QUIT;
				if (selection == MAINENTRY_ABOUT)  status = TITLESTATUS_ABOUT;
				if (selection == MAINENTRY_SELECT) { selection = 0; status = TITLESTATUS_SELECT; };
			default:
				break;
			}
		}
		else if (status == TITLESTATUS_SELECT)
		{
			switch (tevent.key.keysym.sym)
			{
			// nav up and down and vice versa
    		case SDLK_DOWN:
        		selection++;
        		if (selection >= (int)levelnames.size())
            		selection = 0;                                   // am Ende wieder nach oben
        		break;

    		case SDLK_UP:
        		selection--;
        		if (selection < 0 && !levelnames.empty())
            		selection = (int)levelnames.size() - 1;         // von oben ans Ende
        		break;

    		// NEU: ESC im Level-Select
    		case SDLK_ESCAPE:
        		status    = TITLESTATUS_MAIN;  // zurück ins Haupt-Titlescreen-Menü
        		selection = 0;                 // optional: auf ersten Eintrag setzen
        		break;

			case SDLK_SPACE:
			case SDLK_RETURN:
			case SDLK_LCTRL:
				levelout = selection;
				status = TITLESTATUS_MAIN;
				selection = 0;
				return TITLERET_SELECT;
				break;
			default:
				break;
			}
		}
		else
		{
			status = TITLESTATUS_MAIN;
		}
	}

	return TITLERET_NOTHING;
}