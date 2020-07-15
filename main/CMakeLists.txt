# Download dependencies
include(ExternalProject)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/components)
ExternalProject_Add(ESP8266_general_include
  GIT_REPOSITORY  git@https://github.com/notabucketofspam/ESP8266-general-include.git
  GIT_TAG         origin/master
  GIT_PROGRESS    1
  SOURCE_DIR      ${CMAKE_CURRENT_SOURCE_DIR}/components
)
ExternalProject_Add(ESP8266_HTML_GPIO
  GIT_REPOSITORY  git@https://github.com/notabucketofspam/ESP8266-HTML-GPIO.git
  GIT_TAG         origin/master
  GIT_PROGRESS    1
  SOURCE_DIR      ${CMAKE_CURRENT_SOURCE_DIR}/components
)
ExternalProject_Add(Adafruit_NeoPixel
  GIT_REPOSITORY  git@https://github.com/adafruit/Adafruit_NeoPixel.git
  GIT_TAG         origin/master
  GIT_PROGRESS    1
  SOURCE_DIR      ${CMAKE_CURRENT_SOURCE_DIR}/components
)

# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "main.cpp")
set(COMPONENT_ADD_INCLUDEDIRS "")

register_component()
