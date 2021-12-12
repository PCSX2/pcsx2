# Macro to compile po file
# It based on FindGettext.cmake files.
# The macro was adapted for PCSX2 need. Several pot file, language based on directory instead of file

# Copyright (c) 2007-2009 Kitware, Inc., Insight Consortium
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#  * Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
#  * Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
#  * The names of Kitware, Inc., the Insight Consortium, or the names of
#    any consortium members, or of any contributors, may not be used to
#    endorse or promote products derived from this software without
#    specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

FUNCTION(GETTEXT_CREATE_TRANSLATIONS_PCSX2 _potFile)
	SET(_moFiles)
	GET_FILENAME_COMPONENT(_potBasename ${_potFile} NAME_WE)
	GET_FILENAME_COMPONENT(_absPotFile ${_potFile} ABSOLUTE)

	FOREACH (_currentPoFile ${ARGN})
		GET_FILENAME_COMPONENT(_absFile ${_currentPoFile} ABSOLUTE)
		GET_FILENAME_COMPONENT(_abs_PATH ${_absFile} DIRECTORY)
		GET_FILENAME_COMPONENT(_lang ${_abs_PATH} NAME_WE)
		SET(_moFile ${CMAKE_CURRENT_BINARY_DIR}/${_lang}/${_potBasename}.mo)
		IF (APPLE)
			# On MacOS, we have have to preprocess the po files to remove mnemonics:
			# On Windows, menu items have "mnemonics", the items with a letter underlined that you can use with alt to select menu items.  MacOS doesn't do this.
			# Some languages don't use easily-typable characters, so it's common to add a dedicated character for the mnemonic (e.g. in Japanese on Windows, the File menu would be "ファイル(&F)").
			# On MacOS, these extra letters in parentheses are useless and should be avoided.
			SET(_mnemonicless "${CMAKE_CURRENT_BINARY_DIR}/${_lang}/${_potBasename}.nomnemonic.po")
			SET(_compileCommand
				COMMAND sed -e "\"s/[(]&[A-Za-z][)]//g\"" "${_absFile}" > "${_mnemonicless}"
				COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o ${_moFile} ${_mnemonicless}
				BYPRODUCTS ${_mnemonicless}
			)
		ELSE (APPLE)
			SET(_compileCommand
				COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o ${_moFile} ${_absFile}
			)
		ENDIF (APPLE)

		IF (_currentPoFile MATCHES "\\.git")
			continue()
		ENDIF (_currentPoFile MATCHES "\\.git")

		IF (CMAKE_BUILD_PO)
			ADD_CUSTOM_COMMAND(OUTPUT ${_moFile}
				COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/${_lang}
				COMMAND ${GETTEXT_MSGMERGE_EXECUTABLE} --quiet --update --backup=none -s ${_absFile} ${_absPotFile}
				${_compileCommand}
				DEPENDS ${_absPotFile} ${_absFile}
			)
		ELSE (CMAKE_BUILD_PO)
			ADD_CUSTOM_COMMAND(OUTPUT ${_moFile}
				COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/${_lang}
				${_compileCommand}
				DEPENDS ${_absFile}
			)
		ENDIF (CMAKE_BUILD_PO)

		IF(APPLE)
			target_sources(PCSX2 PRIVATE ${_moFile})
			set_source_files_properties(${_moFile} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/locale/${_lang})
			source_group(Resources/locale/${__lang} FILES ${_moFile})
		ELSEIF(PACKAGE_MODE)
			INSTALL(FILES ${_moFile} DESTINATION ${CMAKE_INSTALL_DATADIR}/PCSX2/resources/locale/${_lang})
		ELSE()
			INSTALL(FILES ${_moFile} DESTINATION ${CMAKE_SOURCE_DIR}/bin/resources/locale/${_lang})
		ENDIF()

		LIST(APPEND _moFiles ${_moFile})

	ENDFOREACH (_currentPoFile)

	IF(NOT LINUX_PACKAGE AND NOT APPLE)
		ADD_CUSTOM_TARGET(translations_${_potBasename} ALL DEPENDS ${_moFiles})
	ENDIF(NOT LINUX_PACKAGE AND NOT APPLE)

ENDFUNCTION(GETTEXT_CREATE_TRANSLATIONS_PCSX2)
