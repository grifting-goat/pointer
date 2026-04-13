set(srcs "main.c"
         "esp_hid_gap.c"
         "esp_hid_driver.c"
         "esp_i2c_driver.c")
set(include_dirs "." "include")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES esp_hid esp_driver_gpio
                       PRIV_REQUIRES nvs_flash esp_driver_i2c)
