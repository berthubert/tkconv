#include "scanmon.hh"

std::map<std::string, decltype(&ZaakScanner::make)> g_scanmakers  =
    {
      {"activiteit", ActiviteitScanner::make},
      {"zaak", ZaakScanner::make},
      {"ksd", KsdScanner::make},
      {"zoek", ZoekScanner::make},
      {"commissie", CommissieScanner::make},
      {"persoon", PersoonScanner::make},
    };
