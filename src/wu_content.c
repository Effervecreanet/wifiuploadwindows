#include "wu_content.h"



const struct _http_resources_pages http_resources_pages[] = {
  {"index", "text/html", "html\\light\\fr\\index", "html\\dark\\fr\\index"},
  {"credits", "text/html", "html\\light\\fr\\credits", "html\\dark\\fr\\credits"},
  {"settings", "text/html", "html\\light\\fr\\settings", "html\\dark\\fr\\settings"},
  {"success", "text/html", "html\\light\\fr\\success", "html\\dark\\fr\\success"}};

const struct _http_resources_images http_resources_images[] = {
  {"favicon.ico", "image/x-icon"},
  {"images/wu_images_light_theme/ewu_bg.png", "image/png"},
  {"images/wu_images_light_theme/ewu_checkedfolder.png", "image/png"},
  {"images/wu_images_light_theme/ewu_openfolder.png", "image/png"},
  {"images/wu_images_light_theme/ewu_quit.png", "image/png"},
  {"images/wu_images_light_theme/ewu_welcome.png", "image/png"},
  {"images/wu_images_light_theme/ewu_send.png", "image/png"},
  {"images/wu_images_light_theme/ewu_ticksuccess.png", "image/png"},
  {"images/wu_images_light_theme/ewu_tickwrong.png", "image/png"},
  {"images/wu_images_light_theme/ewu_top_bg.png", "image/png"},
  {"images/wu_images_light_theme/logo4_effervecreanet.png", "image/png"},
  {"images/wu_images_dark_theme/ewu_bg.png", "image/png"},
  {"images/wu_images_dark_theme/ewu_checkedfolder.png", "image/png"},
  {"images/wu_images_dark_theme/ewu_openfolder.png", "image/png"},
  {"images/wu_images_dark_theme/ewu_quit.png", "image/png"},
  {"images/wu_images_dark_theme/ewu_welcome.png", "image/png"},
  {"images/wu_images_dark_theme/ewu_send.png", "image/png"},
  {"images/wu_images_dark_theme/ewu_ticksuccess.png", "image/png"},
  {"images/wu_images_dark_theme/ewu_tickwrong.png", "image/png"},
  {"images/wu_images_dark_theme/ewu_top_bg.png", "image/png"},
  {"images/wu_images_dark_theme/logo4_effervecreanet.png", "image/png"}};

const char http_resource_cmd[2][16] = {
  {"quit"},
  {"openRep"},
};