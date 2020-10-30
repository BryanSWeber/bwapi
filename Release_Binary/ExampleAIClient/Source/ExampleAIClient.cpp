#include <BWAPI.h>
#include <BWAPI/Client.h>

#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <fstream>

#define RESOLUTION (10*24)
#define VISION_DIAGNOSTIC false

using namespace BWAPI;

int main(int argc, const char * argv[]);
void drawStats();
void drawBullets();
void printUnitEventDetails(BWAPI::Event e);
void printUnitDetails(BWAPI::Unit u); //Information about a unit
void printPlayerVision(BWAPI::Player p); // Information about total player visiblity, estimated.
void printPlayerDetails(BWAPI::Player p); // Information about a player's status, upgrades/researches, resources.
void printGameDetails(Event e); //Information about the game's initial conditions and end conditions.
int estimateVisionTiles(Player p); //for a specific player, estimated.
void drawVisibilityData(); // Built-in Diagnostics shows all visiblity for all players, cannot distinguish for whom each tile is visible for.
void showPlayers();
void showForces();
bool show_bullets;
bool show_visibility_data;
std::string EventString(BWAPI::Event e);
Player getEnemy(Player p);

std::string prettyRepName(); //Takes just the replay name, drops '.rep'
bool isOnScreen(const Position p); //Diagnostics, returns true if a position is on screen.

void reconnect()
{
  while(!BWAPIClient.connect())
  {
    std::this_thread::sleep_for(std::chrono::milliseconds{ 1000 });
  }
}

int main(int argc, const char* argv[])
{
    std::cout << "Connecting..." << std::endl;;
    reconnect();
    while (true)
    {
        std::cout << "waiting to enter match" << std::endl;
        while (!Broodwar->isInGame())
        {
            BWAPI::BWAPIClient.update();
            if (!BWAPI::BWAPIClient.isConnected())
            {
                std::cout << "Reconnecting..." << std::endl;;
                reconnect();
            }
        }
        std::cout << "starting match!" << std::endl;
        Broodwar->sendText("Hello world!");
        Broodwar << "The map is " << Broodwar->mapName() << ", a " << Broodwar->getStartLocations().size() << " player map" << std::endl;
        // Enable some cheat flags
        Broodwar->enableFlag(Flag::UserInput);
        // Uncomment to enable complete map information
        //Broodwar->enableFlag(Flag::CompleteMapInformation);

        show_bullets = false;
        show_visibility_data = false;

        //Store the players
        std::vector<Player> players;
        for (auto p : Broodwar->getPlayers()) {
            if (!p->isNeutral()) {
                players.push_back(p);
            }
        }

        if (Broodwar->isReplay())
        {
            Broodwar << "The following players are in this replay:" << std::endl;;
            for (auto p : players)
            {
                if (!p->getUnits().empty() && !p->isNeutral())
                    Broodwar << p->getName() << ", playing as " << p->getRace() << std::endl;
            }
        }

        while (Broodwar->isReplay())
        {
            for (auto &e : Broodwar->getEvents())
            {
                switch (e.getType())
                {
                case EventType::MatchFrame:
                    break; // Do nothing on matchframe events.
                case EventType::MatchEnd:
                    printGameDetails(e);
                default:
                    printUnitEventDetails(e);
                }
            }

            if (VISION_DIAGNOSTIC) {
                for (auto p : Broodwar->getPlayers()) {
                    if (!p->isNeutral()) {
                        printPlayerVision(p);
                    }
                }
            }

            //Print out all unit information every RESOLUTION frames.
            if (Broodwar->getFrameCount() % RESOLUTION == 0) {
                for (auto p : Broodwar->getPlayers()) {
                    if (!p->isNeutral()) {
                        printPlayerDetails(p);
                        printPlayerVision(p);
                        for (auto u : p->getUnits()) {
                            printUnitDetails(u);
                        }
                    }
                }
            }

            if (show_bullets)
                drawBullets();

            if (show_visibility_data)
                drawVisibilityData();

            //drawStats();
            Broodwar->drawTextScreen(300, 0, "FPS: %f", Broodwar->getAverageFPS());

            BWAPI::BWAPIClient.update();
            if (!BWAPI::BWAPIClient.isConnected())
            {
                std::cout << "Reconnecting..." << std::endl;
                reconnect();
            }
        }
        while (Broodwar->isInGame()) {
            //Nothing!
        }
        std::cout << "Game ended" << std::endl;
    }
    std::cout << "Press ENTER to continue..." << std::endl;
    std::cin.ignore();
    return 0;
}

void drawStats()
{
  int line = 0;
  Broodwar->drawTextScreen(5, 0, "I have %d units:", Broodwar->self()->allUnitCount() );
  for (auto& unitType : UnitTypes::allUnitTypes())
  {
    int count = Broodwar->self()->allUnitCount(unitType);
    if ( count )
    {
      Broodwar->drawTextScreen(5, 16*line, "- %d %s%c", count, unitType.c_str(), count == 1 ? ' ' : 's');
      ++line;
    }
  }
}

void drawBullets()
{
  for (auto &b : Broodwar->getBullets())
  {
    Position p = b->getPosition();
    double velocityX = b->getVelocityX();
    double velocityY = b->getVelocityY();
    Broodwar->drawLineMap(p, p + Position((int)velocityX, (int)velocityY), b->getPlayer() == Broodwar->self() ? Colors::Green : Colors::Red);
    Broodwar->drawTextMap(p, "%c%s", b->getPlayer() == Broodwar->self() ? Text::Green : Text::Red, b->getType().c_str());
  }
}

void printUnitEventDetails(BWAPI::Event e)
{
    std::ofstream fout;  // Create Object of Ofstream
    std::ifstream fin;
    std::string filename = prettyRepName() + static_cast<std::string>("-Events") + static_cast<std::string>(".csv");

    fin.open(filename);
    std::string line;
    int csv_length = 0;
    while (getline(fin, line)) {
        ++csv_length;
    }
    fin.close(); // I have read the entire file already, need to close it and begin again.  Lacks elegance, but works.

    if (csv_length < 1) {
        fout.open(filename, std::ios::app); // Append mode
        fout << "EventType" << ","
            << "X.Pos" << "," << "Y.Pos" << ","
            << "UnitType" << ","
            << "UnitOwner" << ","
            << "UnitX.Pos" << "," << "UnitY.Pos" << ","
            << "FrameCount" << ","
            << "UnitID" << "\n";
        fin.close();
    }


    fin.open(filename);
    fout.open(filename, std::ios::app); // Append mode
    if (fin.is_open())
        fout << EventString(e) << ","
        << e.getPosition().x << "," << e.getPosition().y << ","
        << (e.getUnit() ? e.getUnit()->getType().c_str() : "No Unit") << ","
        << (e.getPlayer() ? e.getPlayer()->getName() : "No Player") << ","
        << (e.getUnit() ? std::to_string(e.getUnit()->getPosition().x).c_str() : "No X" ) << "," << (e.getUnit() ? std::to_string(e.getUnit()->getPosition().y).c_str() : "No Y") << ","
        <<  Broodwar->getFrameCount() << "\n"; // Writing data to file
    fin.close();
    fout.close(); // Closing the file
}

void drawVisibilityData()
{
  int wid = Broodwar->mapHeight(), hgt = Broodwar->mapWidth();
  for ( int x = 0; x < wid; ++x )
    for ( int y = 0; y < hgt; ++y )
    {
      if ( Broodwar->isExplored(x, y) )
        Broodwar->drawDotMap(x*32+16, y*32+16, Broodwar->isVisible(x, y) ? Colors::Green : Colors::Blue);
      else
        Broodwar->drawDotMap(x*32+16, y*32+16, Colors::Red);
    }
}

void showPlayers()
{
  Playerset players = Broodwar->getPlayers();
  for(auto p : players)
    Broodwar << "Player [" << p->getID() << "]: " << p->getName() << " is in force: " << p->getForce()->getName() << std::endl;
}

void showForces()
{
  Forceset forces=Broodwar->getForces();
  for(auto f : forces)
  {
    Playerset players = f->getPlayers();
    Broodwar << "Force " << f->getName() << " has the following players:" << std::endl;
    for(auto p : players)
      Broodwar << "  - Player [" << p->getID() << "]: " << p->getName() << std::endl;
  }
}

void printUnitDetails(BWAPI::Unit u) {
    std::ofstream fout;  // Create Object of Ofstream
    std::ifstream fin;
    std::string filename = prettyRepName() + static_cast<std::string>("-Units") + static_cast<std::string>(".csv");

    Player enemy = getEnemy(u->getPlayer());

    fin.open(filename);
    std::string line;
    int csv_length = 0;
    while (getline(fin, line)) {
        ++csv_length;
    }
    fin.close(); // I have read the entire file already, need to close it and begin again.  Lacks elegance, but works.

    if (csv_length < 1) {
        fout.open(filename, std::ios::app); // Append mode
        fout << "unitType" << ","
            << "X.Pos" << "," << "Y.Pos" << ","
            << "PlayerName" << ","
            << "ShownToEnemy" << ","
            << "FrameCount" << ","
            << "Cloaked" << ","
            << "Detected" << ","
            << "HP" << ","
            << "Shields" << ","
            << "Energy" << ","
            << "UnitID" << "\n";
        fin.close();
    }

    fin.open(filename);
    fout.open(filename, std::ios::app); // Append mode
    if (fin.is_open())
        fout << u->getType().c_str() << ","  
        << u->getPosition().x << "," << u->getPosition().y << "," 
        << u->getPlayer()->getName().c_str() << ","  
        << u->isVisible(enemy) << ","  
        << Broodwar->getFrameCount() << ","
        << u->isCloaked() << "," 
        << u->isDetected() << ","
        << u->getHitPoints() << "," 
        << u->getShields() << "," 
        << u->getEnergy() << ","
        << u << "\n"; // Writing data to file
    fin.close();
    fout.close(); // Closing the file
}

void printPlayerVision(BWAPI::Player p) {
    std::ofstream fout;  // Create Object of Ofstream
    std::ifstream fin;
    std::string filename = prettyRepName() + static_cast<std::string>("-Vision") + static_cast<std::string>(".csv");

    fin.open(filename);
    std::string line;
    int csv_length = 0;
    while (getline(fin, line)) {
        ++csv_length;
    }
    fin.close(); // I have read the entire file already, need to close it and begin again.  Lacks elegance, but works.

    if (csv_length < 1) {
        fout.open(filename, std::ios::app); // Append mode
        fout << "PlayerName" << ","
            << "FrameCount" << ","
            << "VisionTiles" << "\n"; // Writing data to file
        fin.close();
    }

    fin.open(filename);
    fout.open(filename, std::ios::app); // Append mode
    if (fin.is_open())
        fout << p->getName().c_str() << ","
        << Broodwar->getFrameCount() << ","
        << estimateVisionTiles(p) << "\n"; // Writing data to file
    fin.close();
    fout.close(); // Closing the file
}

void printPlayerDetails(BWAPI::Player p) {
    std::ofstream fout;  // Create Object of Ofstream
    std::ifstream fin;
    std::string filename = prettyRepName() + static_cast<std::string>("-Players") + static_cast<std::string>(".csv");

    fin.open(filename);
    std::string line;
    int csv_length = 0;
    while (getline(fin, line)) {
        ++csv_length;
    }
    fin.close(); // I have read the entire file already, need to close it and begin again.  Lacks elegance, but works.

    std::string upgradeNameString;
    std::string upgradeString;
    for (auto u : UpgradeTypes::allUpgradeTypes()) {
        upgradeNameString += u.c_str() + static_cast<std::string>(",");
        int level = p->getUpgradeLevel(u);

        if (p->isUpgrading(u))
            upgradeString += "RESEARCHING " + std::to_string(++level) + static_cast<std::string>(",");
        else
            upgradeString += std::to_string(p->getUpgradeLevel(u)) + static_cast<std::string>(",");
    }

    std::string techNameString;
    std::string techString;
    for (auto u : TechTypes::allTechTypes()) {
        techNameString += u.c_str() + static_cast<std::string>(",");
        if (p->hasResearched(u))
            techString += "DONE" + static_cast<std::string>(",");
        else if (p->isResearching(u))
            techString += "RESEARCHING" + static_cast<std::string>(",");
        else
            techString += "0" + static_cast<std::string>(",");
    }


    if (csv_length < 1) {
        fout.open(filename, std::ios::app); // Append mode
        fout << "PlayerName" << ","
            << "FrameCount" << ","
            << "Minerals" << ","
            << "Gas" << ","
            << "SupplyTotal" << ","
            << "SupplyUsed" << ","
            << upgradeNameString << "\n"; // Writing data to file
        fin.close();
    }

    fin.open(filename);
    fout.open(filename, std::ios::app); // Append mode
    if (fin.is_open())
        fout << p->getName().c_str() << ","
        << Broodwar->getFrameCount() << ","
        << p->minerals() << ","
        << p->gas() << "," 
        << p->supplyTotal() << "," 
        << p->supplyUsed() << "," 
        << upgradeString << "\n"; // Writing data to file
    fin.close();
    fout.close(); // Closing the file
}

void printGameDetails(Event e) {
    std::ofstream fout;  // Create Object of Ofstream
    std::ifstream fin;
    std::string filename = prettyRepName() + static_cast<std::string>("-Game") + static_cast<std::string>(".csv");

    fin.open(filename);
    std::string line;
    int csv_length = 0;
    while (getline(fin, line)) {
        ++csv_length;
    }
    fin.close(); // I have read the entire file already, need to close it and begin again.  Lacks elegance, but works.

    if (csv_length < 1) {
        fout.open(filename, std::ios::app); // Append mode
        fout << "PlayerName" << ","
            << "Race" << ","
            << "BuildScore" << ","
            << "RazeScore" << ","
            << "UnitScore" << ","
            << "LeftGame" << ","
            << "DeclaredWin" << ","
            << "MapName" << "\n"; // Writing data to file
        fin.close();
    }

    for (auto p : Broodwar->getPlayers()) {
        fin.open(filename);
        fout.open(filename, std::ios::app); // Append mode
        if (fin.is_open())
            fout << p->getName().c_str() << ","
            << p->getRace() << ","
            << p->getBuildingScore() << ","
            << p->getRazingScore() << ","
            << p->getUnitScore() << ","
            << p->leftGame() << "," 
            << p->isVictorious() << ","
            << Broodwar->mapName().c_str() << "\n"; // Writing data to file
        fin.close();
        fout.close(); // Closing the file
    }
}

int estimateVisionTiles(Player p) {
    int map_x = BWAPI::Broodwar->mapWidth();
    int map_y = BWAPI::Broodwar->mapHeight();
    double visibility[256][256] = { 0.0 };

    int vision_tile_count = 0;
    int max_visibilty = 0;
    for (int tile_x = 1; tile_x <= map_x; tile_x++) { // there is no tile (0,0)
        for (int tile_y = 1; tile_y <= map_y; tile_y++) {
            for (auto u : Broodwar->getUnitsOnTile(tile_x,tile_y)) {
                if (u->getPlayer() == p && !u->isBlind() && !u->isMorphing() && !u->isConstructing() && u->getType().sightRange()) {
                    int sight = u->getType().sightRange() / 32 + 1.0;
                    visibility[tile_x][tile_y] = std::max(sight, static_cast<int>(visibility[tile_x][tile_y]) );
                    max_visibilty = std::max(sight, max_visibilty );
                }
            }
        }
    } // this search must be very exhaustive to do every frame. But C++ does it without any problems.

    max_visibilty++; // buffer of one foot

    while (max_visibilty >= 0) {
        for (int tile_x = 1; tile_x <= map_x; tile_x++) { // Ignore row 0
            for (int tile_y = 1; tile_y <= map_y; tile_y++) {
                double lateral_tiles = std::max({
                    visibility[tile_x + 1][tile_y],
                    visibility[tile_x - 1][tile_y],
                    visibility[tile_x][tile_y + 1],
                    visibility[tile_x][tile_y - 1]
                    });
                double diagonal_tiles = std::max({
                    visibility[tile_x + 1][tile_y + 1],
                    visibility[tile_x + 1][tile_y - 1],
                    visibility[tile_x - 1][tile_y + 1],
                    visibility[tile_x - 1][tile_y - 1]
                    });
                visibility[tile_x][tile_y] = std::max({ lateral_tiles - 1.0, diagonal_tiles - sqrt(2.0), visibility[tile_x][tile_y], 0.0 }); //0.999 is because unit vision seems to start a hair beyond its starting tile.  Corrects a vision imbalance on the outermost fvision radius.
            }
        }
        max_visibilty--;
    }

    for (int tile_x = 1; tile_x <= map_x; tile_x++) { // Ignore row 0
        for (int tile_y = 1; tile_y <= map_y; tile_y++) {
            if (visibility[tile_x][tile_y] > 0.0) {
                vision_tile_count++;
                Position pos = Position(TilePosition(tile_x, tile_y));
                Position plus_one_pos = Position(TilePosition(tile_x + 1, tile_y + 1));
                if (isOnScreen(pos) && VISION_DIAGNOSTIC) {
                    Broodwar->drawBoxMap(pos, plus_one_pos, Colors::Green, false);
                    Broodwar->drawTextMap(pos, "%2.2f", visibility[tile_x][tile_y]);
                }
            }
        }
    }

    return vision_tile_count;
}


std::string EventString(BWAPI::Event e)
{
    const char* s = 0;
    switch (e.getType()) {
    case EventType::MatchStart:
        s = "MatchStart";
        break;

    case EventType::MatchEnd:
        s = "MatchEnd";
        break;

    case EventType::MatchFrame:
        s = "MatchFrame";
        break;

    case EventType::MenuFrame:
        s = "MenuFrame";
        break;

    case EventType::SendText:
        s = "SendText";
        break;

    case EventType::ReceiveText:
        s = "ReceiveText";
        break;

    case EventType::PlayerLeft:
        s = "PlayerLeft";
        break;

    case EventType::NukeDetect:
        s = "NukeDetect";
        break;

    case EventType::UnitDiscover:
        s = "UnitDiscover";
        break;

    case EventType::UnitEvade:
        s = "UnitEvade";
        break;

    case EventType::UnitShow:
        s = "UnitShow";
        break;

    case EventType::UnitHide:
        s = "UnitHide";
        break;

    case EventType::UnitCreate:
        s = "UnitCreate";
        break;

    case EventType::UnitDestroy:
        s = "UnitDestroy";
        break;

    case EventType::UnitMorph:
        s = "UnitMorph";
        break;

    case EventType::UnitRenegade:
        s = "UnitRenegade";
        break;

    case EventType::SaveGame:
        s = "SaveGame";
        break;

    case EventType::UnitComplete:
        s = "UnitComplete";
        break;

        //TriggerAction,
    case EventType::None:
        s = "None";
        break;

    }

    return s;
}

Player getEnemy(Player p) {
    for (auto e : Broodwar->getPlayers()) {
        if (e->isEnemy(p)) {
            return e;
            break;
        }
    }
}

std::string prettyRepName() {
    std::string body = Broodwar->mapFileName().c_str();
    std::string clean = body.substr(0,body.find("."));
    return clean;
}

bool isOnScreen(const Position pos) {
    bool inrange_x = Broodwar->getScreenPosition().x < pos.x && Broodwar->getScreenPosition().x + 640 > pos.x;
    bool inrange_y = Broodwar->getScreenPosition().y < pos.y && Broodwar->getScreenPosition().y + 480 > pos.y;
    return inrange_x && inrange_y;
}