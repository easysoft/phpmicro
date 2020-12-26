
micro: $(SAPI_MICRO_PATH)

micro_2s_objs_clean:
	-rm $(MICRO_2STAGE_OBJS)

micro_2s_objs: micro_2s_objs_clean $(MICRO_2STAGE_OBJS)

$(SAPI_MICRO_PATH): $(PHP_GLOBAL_OBJS) $(PHP_BINARY_OBJS) $(PHP_MICRO_OBJS)
	make micro_2s_objs SFX_FILESIZE=0xcafebabe
	$(BUILD_MICRO)
	$(STRIP) $(MICRO_STRIP_FLAGS) $(SAPI_MICRO_PATH)
	make micro_2s_objs SFX_FILESIZE=`$(STAT_SIZE) $(SAPI_MICRO_PATH)`
	$(BUILD_MICRO)
	$(STRIP) $(MICRO_STRIP_FLAGS) $(SAPI_MICRO_PATH)

sfx_test:
	echo test
