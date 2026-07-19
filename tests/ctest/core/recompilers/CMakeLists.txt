# In-process differential test harness for the PS2 recompilers.
#
# Each test loads a short MIPS-I (IOP) or MIPS-III (EE) program at
# kProgramPc, seeds register state, runs through both the JIT and the
# interpreter, captures full architectural state, and gtest-diffs the
# two — any divergence is a JIT bug. See harness/JitTestHarness.h
# (IOP) and harness/EeRecTestHarness.h (EE) for the contracts.
#
# The EE harness drives the JIT via recEeExecuteBlock(cycles, park_pc),
# a bounded-cycle entry point alongside the production recExecute().
# recEeIsBlockLinked(src_pc, dst_pc) backs the multiblock tests'
# block-link introspection.
add_pcsx2_test(recompiler_tests
	main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../StubHost.cpp
	harness/RecompilerTestEnvironment.cpp
	harness/JitTestHarness.cpp
	harness/EeRecTestHarness.cpp
	harness/StateSnapshot.cpp
	harness/VuSnapshot.cpp
	harness/VuTestHarness.cpp
	harness/VuReplay.cpp
	iop_alu_tests.cpp
	iop_jit_fuzz_tests.cpp
	iop_shift_tests.cpp
	iop_muldiv_tests.cpp
	iop_loadstore_tests.cpp
	iop_jump_tests.cpp
	iop_branch_tests.cpp
	iop_cop0_tests.cpp
	iop_memory_access_tests.cpp
	iop_regalloc_pressure_tests.cpp
	iop_multiblock_tests.cpp
	iop_smc_tests.cpp
	iop_cop0_exception_tests.cpp
	iop_irx_hle_tests.cpp
	arm64_baseblocks_link_tests.cpp
	ee_rec_alu_tests.cpp
	ee_rec_alu64_tests.cpp
	ee_rec_fuzz_tests.cpp
	ee_rec_alu_imm_tests.cpp
	ee_rec_branch_tests.cpp
	ee_rec_loop_residency_tests.cpp
	ee_rec_superblock_tests.cpp
	ee_rec_branch_in_delay_tests.cpp
	ee_rec_callret_tests.cpp
	ee_rec_cop0_tests.cpp
	ee_rec_fpu_tests.cpp
	ee_rec_fpu_full_mode_tests.cpp
	ee_rec_fpu_guardbit_tests.cpp
	ee_rec_fpu_rsqrt_tests.cpp
	ee_rec_harness_validation_tests.cpp
	ee_rec_gem2_coherence_tests.cpp
	ee_rec_iop_handoff_tests.cpp
	ee_rec_jump_tests.cpp
	ee_rec_loadstore_tests.cpp
	ee_rec_mmi_coherence_tests.cpp
	ee_rec_mmi_simd_tests.cpp
	ee_rec_mmi_tests.cpp
	ee_rec_move_tests.cpp
	ee_rec_pinned_gpr_tests.cpp
	ee_rec_regalloc_coupling_tests.cpp
	ee_rec_muldiv_tests.cpp
	ee_rec_multiblock_tests.cpp
	ee_rec_shift_tests.cpp
	ee_rec_skipmpeg_tests.cpp
	ee_rec_smc_manual_tests.cpp
	ee_rec_smc_tests.cpp
	ee_rec_timeout_loop_tests.cpp
	ee_rec_traps_tests.cpp
	vtlb_get_guest_address_tests.cpp
	vu0_harness_validation_tests.cpp
	vu0_alu_upper_tests.cpp
	vu0_alu_lower_tests.cpp
	vu0_q_pipeline_tests.cpp
	vu0_integer_alu_tests.cpp
	vu0_branch_delay_tests.cpp
	vu0_flag_pipeline_tests.cpp
	vu_end_program_status_broadcast_tests.cpp
	vu0_e_d_t_m_bit_tests.cpp
	vu0_clamp_modes_tests.cpp
	vu_ftoi_saturation_tests.cpp
	vu_madda_acc_lane_tests.cpp
	vu_overflow_hack_tests.cpp
	ee_vu0_cfc2_ctc2_tests.cpp
	ee_vu0_qmfc2_qmtc2_tests.cpp
	ee_vu0_cop2_macro_tests.cpp
	vu1_alu_upper_tests.cpp
	vu1_alu_lower_tests.cpp
	vu1_efu_p_pipeline_tests.cpp
	vu1_xgkick_tests.cpp
	ee_vu1_vif_dispatch_tests.cpp
	vu_capture_format_tests.cpp
	vu_replay_tests.cpp
	mvu_observed_entries_tests.cpp
	mvu_cache_exhaustion_tests.cpp
	mvu_lpstate_invariant_tests.cpp
	mvu_resume_dispatch_tests.cpp
	mvu_spin_ff_tests.cpp
	mvu_contentmap_drift_tests.cpp
	mvu_lane_fmul_fold_tests.cpp
	mvu_abi_digest_tests.cpp
	mvu_persist_roundtrip_tests.cpp
	mvu_progcache_disk_tests.cpp
	mvu_waitmtvu_stub_tests.cpp
)

target_include_directories(recompiler_tests PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(recompiler_tests PUBLIC
	PCSX2_FLAGS
	PCSX2
	common
)

# mVUProgCache VERSION-handshake / stale-cache eviction / INDEX-format
# tests. Drives mVUProgCache::Init through a temp EmuFolders::Cache and
# asserts that poisoned on-disk VERSION fields (magic / formatVersion /
# compilerAbiVersion / optionsSentinel) all rename the cache aside
# before the new run accepts INDEX entries, and that the variable-length
# INDEX serializer round-trips / drops truncated tails / skips
# tombstones. The disk scaffolding is the foundation of the
# persisted-JIT program cache.
add_pcsx2_test(mvu_progcache_versioning_tests
	${CMAKE_CURRENT_SOURCE_DIR}/../StubHost.cpp
	mvu_progcache_versioning_tests.cpp
)
target_include_directories(mvu_progcache_versioning_tests PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/..
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../pcsx2
)
target_link_libraries(mvu_progcache_versioning_tests PUBLIC
	PCSX2_FLAGS
	PCSX2
	common
)
