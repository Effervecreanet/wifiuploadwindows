#include <Windows.h>
#include "wu_content.h"



const struct _http_resources http_resources[] = {
  {"index", "text/html", "html\\light\\index", "html\\dark\\index"},
  {"favicon.ico", "image/x-icon", "images\\favicon.ico", "images\\favicon.ico"},
  {"credits", "text/html", "html\\light\\credits", "html\\dark\\credits"},
  {"settings", "text/html", "html\\light\\settings", "html\\dark\\settings"},
  {"success", "text/html", "html\\light\\success", "html\\dark\\success"},
  {"erreur_404", "text/html", "html\\light\\erreur_404", "html\\dark\\erreur_404"},
  {"erreur_fichier_nul", "text/html", "html\\light\\erreur_fichier_nul", "html\\dark\\erreur_fichier_nul"},
  {"theme", "", "", ""},
  {"upload", "", "", ""},
  {"images/ewu_bg.png", "image/png", "images\\wu_images_light_theme\\ewu_bg.png", "images\\wu_images_dark_theme\\ewu_bg.png"},
  {"images/ewu_checkedfolder.png", "image/png", "images\\wu_images_light_theme\\ewu_checkedfolder.png", "images\\wu_images_dark_theme\\ewu_checkedfolder.png"},
  {"images/ewu_openfolder.png", "image/png", "images\\wu_images_light_theme\\ewu_openfolder.png", "images\\wu_images_dark_theme\\ewu_openfolder.png"},
  {"images/ewu_quit.png", "image/png", "images\\wu_images_light_theme\\ewu_quit.png", "images\\wu_images_dark_theme\\ewu_quit.png"},
  {"images/ewu_welcome.png", "image/png", "images\\wu_images_light_theme\\ewu_welcome.png", "images\\wu_images_dark_theme\\ewu_welcome.png"},
  {"images/ewu_send.png", "image/png", "images\\wu_images_light_theme\\ewu_send.png", "images\\wu_images_dark_theme\\ewu_send.png"},
  {"images/ewu_ticksuccess.png", "image/png", "images\\wu_images_light_theme\\ewu_ticksuccess.png", "images\\wu_images_dark_theme\\ewu_ticksuccess.png"},
  {"images/eloge-merite-IA.png", "image/png", "images\\wu_images_light_theme\\eloge-merite-IA.png", "images\\wu_images_dark_theme\\eloge-merite-IA.png"},
  {"images/illustration-parametre-theme.png", "image/png", "images\\wu_images_light_theme\\illustration-parametre-theme.png", "images\\wu_images_dark_theme\\illustration-parametre-theme.png"},
  {"images/ewu_tickwrong.png", "image/png", "images\\wu_images_light_theme\\ewu_tickwrong.png", "images\\wu_images_dark_theme\\ewu_tickwrong.png"},
  {"images/ewu_top_bg.png", "image/png", "images\\wu_images_light_theme\\ewu_top_bg.png", "images\\wu_images_dark_theme\\ewu_top_bg.png"},
  {"images/logo6_effervecreanet.png", "image/png", "images\\wu_images_light_theme\\logo6_effervecreanet.png", "images\\wu_images_dark_theme\\logo6_effervecreanet.png"},
  {"quit", NULL, NULL, NULL},
  {"openRep", NULL, NULL, NULL},
  {NULL, NULL, NULL, NULL}
};
