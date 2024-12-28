# https://github.com/eliemichel/WebGPU-distribution/blob/main/webgpu.cmake
macro(FetchContent_DeclareShallowGit Name GIT_REPOSITORY GitRepository GIT_TAG GitTag)
	FetchContent_Declare(
		"${Name}"
		DOWNLOAD_COMMAND
			cd "${FETCHCONTENT_BASE_DIR}/${Name}-src" &&
			git init &&
			git fetch --depth=1 "${GitRepository}" "${GitTag}" &&
			git reset --hard FETCH_HEAD
	)
endmacro()