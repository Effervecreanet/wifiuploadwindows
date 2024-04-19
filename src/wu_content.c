#include "wu_content.h"



const struct _http_resources http_resources[] = {
  {"index", "text/html", "html\\light\\index", "html\\dark\\index"},
  {"favicon.ico", "image/x-icon", NULL, NULL},  
  {"credits", "text/html", "html\\light\\credits", "html\\dark\\credits"},
  {"settings", "text/html", "html\\light\\settings", "html\\dark\\settings"},
  {"success", "text/html", "html\\light\\success", "html\\dark\\success"}};
  {"erreur_404", "text/html", "html\\light\\erreur_404", "html\\dark\\erreur_404"}};  
  {"erreur_fichier_nul", "text/html", "html\\light\\erreur_fichier_nul", "html\\dark\\erreur_fichier_nul"}};    
  {"images/wu_images_light_theme/ewu_bg.png", "image/png", NULL, NULL},
  {"images/wu_images_light_theme/ewu_checkedfolder.png", "image/png", NULL, NULL},
  {"images/wu_images_light_theme/ewu_openfolder.png", "image/png", NULL, NULL},
  {"images/wu_images_light_theme/ewu_quit.png", "image/png", NULL, NULL},
  {"images/wu_images_light_theme/ewu_welcome.png", "image/png", NULL, NULL},
  {"images/wu_images_light_theme/ewu_send.png", "image/png", NULL, NULL},
  {"images/wu_images_light_theme/ewu_ticksuccess.png", "image/png", NULL, NULL},
  {"images/wu_images_light_theme/ewu_tickwrong.png", "image/png", NULL, NULL},
  {"images/wu_images_light_theme/ewu_top_bg.png", "image/png", NULL, NULL},
  {"images/wu_images_light_theme/logo4_effervecreanet.png", "image/png", NULL, NULL},
  {"images/wu_images_dark_theme/ewu_bg.png", "image/png", NULL, NULL},
  {"images/wu_images_dark_theme/ewu_checkedfolder.png", "image/png", NULL, NULL},
  {"images/wu_images_dark_theme/ewu_openfolder.png", "image/png", NULL, NULL},
  {"images/wu_images_dark_theme/ewu_quit.png", "image/png", NULL, NULL},
  {"images/wu_images_dark_theme/ewu_welcome.png", "image/png", NULL, NULL},
  {"images/wu_images_dark_theme/ewu_send.png", "image/png", NULL, NULL},
  {"images/wu_images_dark_theme/ewu_ticksuccess.png", "image/png", NULL, NULL},
  {"images/wu_images_dark_theme/ewu_tickwrong.png", "image/png", NULL, NULL},
  {"images/wu_images_dark_theme/ewu_top_bg.png", "image/png", NULL, NULL},
  {"images/wu_images_dark_theme/logo4_effervecreanet.png", "image/png", NULL, NULL},
  {"quit", NULL, NULL, NULL},
  {"openRep", NULL, NULL, NULL},
  {NULL, NULL, NULL, NULL}
};