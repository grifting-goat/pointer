set(srcs "main.c"
         "esp_hid_gap.c"
         "esp_hid_driver.c"
         "esp_i2c_driver.c"
         "cnn_model.cpp"
        )
set(include_dirs "." "include")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES esp_hid esp_driver_gpio esp_nn esp_dl
                       PRIV_REQUIRES nvs_flash esp_driver_i2c)

set(image_file "${CMAKE_CURRENT_SOURCE_DIR}/model/cnn.espdl")
esptool_py_flash_to_partition(flash "model" "${image_file}")
  