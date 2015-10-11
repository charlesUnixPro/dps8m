#!/bin/bash

T=./source/
S3=./MR12.3+5/
S5=./MR12.5/
TAPES=/home/cac/Projects/multics/bitsavers.trailing-edge.com/bits/Honeywell/multics/tape/

../tapeUtils/restore_tape $S3 $TAPES/88534.tap $TAPES/88631.tap
../tapeUtils/restore_tape $S3 $TAPES/98570.tap $TAPES/99019.tap
../tapeUtils/restore_tape $S3 $TAPES/88632.tap $TAPES/88633.tap $TAPES/88634.tap $TAPES/88635.tap $TAPES/88636.tap $TAPES/99020.tap
../tapeUtils/restore_tape $S3 $TAPES/93085.tap

../tapeUtils/restore_tape $S5 $TAPES/20185.tap
../tapeUtils/restore_tape $S5 $TAPES/20186.tap
../tapeUtils/restore_tape $S5 $TAPES/20188.tap
../tapeUtils/restore_tape $S5 $TAPES/20187.tap

# move things from MR12.5 into MR12.3

echo "Start merge"


#mv $S5/documentation/MR12.5/* $S3/documentation/
cp -r $S5/documentation/MR12.5/* $S3/documentation/
rm -r $S5/documentation/MR12.5/ 
rmdir $S5/documentation 

cp -r $S5/library_dir_dir/crossref $S3/library_dir_dir
rm -r $S5/library_dir_dir/crossref

cp -r $S5/library_dir_dir/system_library_1/info/ $S3/library_dir_dir/system_library_1/
rm -r $S5/library_dir_dir/system_library_1/info
rmdir $S5/library_dir_dir/system_library_1 

mkdir $S3/library_dir_dir/MR12.5/
mv $S5/library_dir_dir/MR12.5/12.5.C_COMPILER/bound_cc_commands $S3/system_library_3rd_party/C_COMPILER/executable/
mv $S5/library_dir_dir/MR12.5/12.5.C_COMPILER/ccom $S3/system_library_3rd_party/C_COMPILER/executable/
mv $S5/library_dir_dir/MR12.5/12.5.C_COMPILER/main_ $S3/system_library_3rd_party/C_COMPILER/executable/
mv $S5/library_dir_dir/MR12.5/12.5.C_COMPILER/bound_cc_commands.archive $S3/system_library_3rd_party/C_COMPILER/object/
mv $S5/library_dir_dir/MR12.5/12.5.C_COMPILER/bound_cc_commands.s.archive $S3/system_library_3rd_party/C_COMPILER/source/
mv $S5/library_dir_dir/MR12.5/12.5.C_COMPILER/cc_info.incl.pl1 $S3/system_library_3rd_party/C_COMPILER/include/
mv $S5/library_dir_dir/MR12.5/12.5.C_COMPILER/install_ccompiler.ec $S3/library_dir_dir/MR12.5/
rmdir $S5/library_dir_dir/MR12.5/12.5.C_COMPILER/

mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_absentee_com_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_active_function_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_command_env_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_command_loop_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_dial_out_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_dm_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_exec_com_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_fs_util_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_fscom1_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_fscom2_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_io_commands_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_kermit_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_mail_system_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_menu_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_misc_commands_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_mrds_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_mseg_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_pl1_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_printing_cmds_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_probe_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_segment_info_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_sort_routines_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_ti_term_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_vfile_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_video_ $S3/system_library_standard/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/ask_ $S3/system_library_tools/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_admin_rtnes_ $S3/system_library_tools/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_admin_tools_ $S3/system_library_tools/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_as_install_ctl_ $S3/system_library_tools/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_as_misc_ $S3/system_library_tools/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_copy_disk_vol_ $S3/system_library_tools/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_dfm_ $S3/system_library_tools/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_gm_ $S3/system_library_tools/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_io_tools_ $S3/system_library_tools/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_iodd_ $S3/system_library_tools/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_misc_translatrs_ $S3/system_library_tools/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_pnotice_ $S3/system_library_tools/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_pnt_interface_ $S3/system_library_tools/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_tuning_ $S3/system_library_tools/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_user_ctl_ $S3/system_library_tools/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/bound_volume_retv_ $S3/system_library_tools/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/nothing $S3/system_library_tools/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/parse.incl.pl1 $S3/library_dir_dir/include/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/ag91.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/ag92.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/ag93.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/ag94.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/ak51.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/al39.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/am83.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/as_who.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/ask_.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/az49.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/cc75.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/cg40.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/ch23.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/ch27.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/cj52.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/cp51.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/cv_integer_string_.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/cv_integer_string_check_.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/cy74.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/date_time_.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/dial_out.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/dx71.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/gb58.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/gb64.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/get_equal_name_.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/hh07.errata.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/io_error_summary.info $S3/documentation/privileged/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/manuals.gi.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/new_user.info $S3/documentation/privileged/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/pl1.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/probe.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/print_sys_log.info $S3/documentation/privileged/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/ssu.ab.info $S3/documentation/subsystem/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/status.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/user.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/wait.info $S3/documentation/subsystem/dial_out/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/window_editor_utils_.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/add_name.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/cancel_daemon_request.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/cancel_output_request.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/decode_definition_.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/delete_name.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/display_forms_info.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/dm_set_free_area.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/exec_com.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/fundamental-mode.info $S3/documentation/subsystem/emacs/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/get_bound_seg_info_.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/get_pathname.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/iox_.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/kermit_modes.gi.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/library_cleanup.info $S3/documentation/privileged
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/links.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/menu_get_choice.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/nothing.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/null_entry_.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/pascal.changes.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/set_mailing_address.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/set_system_console.info $S3/documentation/privileged
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/terminate_refname.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/time_format.gi.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/txn.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/window_io_.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/working_dir.info $S3/documentation/info_segments/
mkdir $S3/documentation/subsystem/xforum/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/xforum_prompt.info $S3/documentation/subsystem/xforum/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE/install_executable.ec $S3/library_dir_dir/MR12.5/
rmdir $S5/library_dir_dir/MR12.5/12.5.EXECUTABLE

mv $S5/library_dir_dir/MR12.5/12.5.EXECUTIVE_MAIL/bound_executive_mail_.s.archive $S3/library_dir_dir/system_library_unbundled/source/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTIVE_MAIL/bound_executive_mail_.archive $S3/library_dir_dir/system_library_unbundled/object/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTIVE_MAIL/bound_executive_mail_ $S3/system_library_unbundled/
mv $S5/library_dir_dir/MR12.5/12.5.EXECUTIVE_MAIL/install_executive_mail.ec $S3/library_dir_dir/MR12.5/
rmdir $S5/library_dir_dir/MR12.5/12.5.EXECUTIVE_MAIL

mv $S5/library_dir_dir/MR12.5/12.5.EXTENDED_MAIL/bound_extended_mail_.s.archive $S3/library_dir_dir/system_library_unbundled/source/
mv $S5/library_dir_dir/MR12.5/12.5.EXTENDED_MAIL/bound_extended_mail_.archive $S3/library_dir_dir/system_library_unbundled/object/
mv $S5/library_dir_dir/MR12.5/12.5.EXTENDED_MAIL/bound_extended_mail_ $S3/system_library_unbundled/
mv $S5/library_dir_dir/MR12.5/12.5.EXTENDED_MAIL/install_extended_mail.ec $S3/library_dir_dir/MR12.5/
rmdir $S5/library_dir_dir/MR12.5/12.5.EXTENDED_MAIL

mv $S5/library_dir_dir/MR12.5/12.5.FORTRAN/*.s.archive $S3/library_dir_dir/system_library_unbundled/source/
mv $S5/library_dir_dir/MR12.5/12.5.FORTRAN/*.archive $S3/library_dir_dir/system_library_unbundled/object/
mv $S5/library_dir_dir/MR12.5/12.5.FORTRAN/bound_fort_ $S3/system_library_unbundled/
mv $S5/library_dir_dir/MR12.5/12.5.FORTRAN/install_fortran.ec $S3/library_dir_dir/MR12.5/
rmdir $S5/library_dir_dir/MR12.5/12.5.FORTRAN

mv $S5/library_dir_dir/MR12.5/12.5.FORUM/*.s.archive $S3/library_dir_dir/system_library_unbundled/source/
mv $S5/library_dir_dir/MR12.5/12.5.FORUM/forum_data_.cds $S3/library_dir_dir/system_library_unbundled/source/
mv $S5/library_dir_dir/MR12.5/12.5.FORUM/*.archive $S3/library_dir_dir/system_library_unbundled/object/
mv $S5/library_dir_dir/MR12.5/12.5.FORUM/bound_forum_ $S3/system_library_unbundled/
mv $S5/library_dir_dir/MR12.5/12.5.FORUM/bound_v2_forum_mgr_ $S3/system_library_unbundled/
mv $S5/library_dir_dir/MR12.5/12.5.FORUM/forum_data_ $S3/system_library_unbundled/
mv $S5/library_dir_dir/MR12.5/12.5.FORUM/forum.info $S3/documentation/info_segments/
mv $S5/library_dir_dir/MR12.5/12.5.FORUM/reset.info $S3/documentation/subsystem/forum/
mv $S5/library_dir_dir/MR12.5/12.5.FORUM/trans_specs.info $S3/documentation/subsystem/forum/
mv $S5/library_dir_dir/MR12.5/12.5.FORUM/forum_trans_list.incl.pl1 $S3/library_dir_dir/include/
mv $S5/library_dir_dir/MR12.5/12.5.FORUM/install_forum.ec $S3/library_dir_dir/MR12.5/
rmdir $S5/library_dir_dir/MR12.5/12.5.FORUM

mv $S5/library_dir_dir/MR12.5/12.5.cleanup_library.ec $S3/library_dir_dir/MR12.5/
mv $S5/library_dir_dir/MR12.5/12.5.install_part1.ec $S3/library_dir_dir/MR12.5/
mv $S5/library_dir_dir/MR12.5/12.5.install_part2.ec $S3/library_dir_dir/MR12.5/
mv $S5/library_dir_dir/MR12.5/update.ec $S3/library_dir_dir/MR12.5/

mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/*.s.archive $S3/library_dir_dir/system_library_1/source/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/*.archive $S3/library_dir_dir/system_library_1/object/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/bound_volume_reloader_ $S3/library_dir_dir/system_library_1/execution/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/bound_system_control_ $S3/library_dir_dir/system_library_1/execution/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/bound_segment_control $S3/library_dir_dir/system_library_1/execution/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/bound_rcprm_ $S3/library_dir_dir/system_library_1/execution/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/bound_hc_tuning $S3/library_dir_dir/system_library_1/execution/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/bound_tty_active $S3/library_dir_dir/system_library_1/execution/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/bound_system_security $S3/library_dir_dir/system_library_1/execution/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/bound_page_control $S3/library_dir_dir/system_library_1/execution/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/bound_library_wired_ $S3/library_dir_dir/system_library_1/execution/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/bound_hc_initlzr_auxl_ $S3/library_dir_dir/system_library_1/execution/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/bound_file_system $S3/library_dir_dir/system_library_1/execution/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/bound_dir_control $S3/library_dir_dir/system_library_1/execution/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/fw.urcmpc.ucrp.b2 $S3/library_dir_dir/firmware/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/fw.urcmpc.ucmn.a2 $S3/library_dir_dir/firmware/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/fw.urcmpc.u400.m1 $S3/library_dir_dir/firmware/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/fw.mtp610.m610.c2 $S3/library_dir_dir/firmware/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/fw.mtp601.m601.t1 $S3/library_dir_dir/firmware/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/fw.mtc500.m500.v1 $S3/library_dir_dir/firmware/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/fw.msp800.msp8.l1 $S3/library_dir_dir/firmware/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/fw.dsc500.d500.y1 $S3/library_dir_dir/firmware/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/fw.dsc191.m191.v1 $S3/library_dir_dir/firmware/
mv $S5/library_dir_dir/MR12.5/12.5.HARDCORE/install_hard.ec $S3/library_dir_dir/MR12.5/
rmdir $S5/library_dir_dir/MR12.5/12.5.HARDCORE

#mv $S5/library_dir_dir/MR12.5/12.5.LDD/*.s.archive $S3/library_dir_dir/system_library_tools/source
#mv $S5/library_dir_dir/MR12.5/12.5.LDD/*.archive $S3/library_dir_dir/system_library_tools/object
mv $S5/library_dir_dir/MR12.5/12.5.LDD/ask_ $S3/library_dir_dir/system_library_tools/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/ask_.pl1 $S3/library_dir_dir/system_library_tools/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/nothing $S3/library_dir_dir/system_library_tools/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/nothing.alm $S3/library_dir_dir/system_library_tools/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/dfm_data.incl.pl1 $S3/library_dir_dir/include/



mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_volume_retv_.s.archive $S3/library_dir_dir/system_library_tools/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_user_ctl_.s.archive $S3/library_dir_dir/system_library_tools/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_pnt_interface_.s.archive $S3/library_dir_dir/system_library_tools/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_io_tools_.s.archive $S3/library_dir_dir/system_library_tools/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_gm_.s.archive $S3/library_dir_dir/system_library_tools/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_dfm_.s.archive $S3/library_dir_dir/system_library_tools/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_copy_disk_vol_.s.archive $S3/library_dir_dir/system_library_tools/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_as_install_ctl_.s.archive $S3/library_dir_dir/system_library_tools/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_admin_tools_.s.archive $S3/library_dir_dir/system_library_tools/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_admin_rtnes_.s.archive $S3/library_dir_dir/system_library_tools/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_pl1_.[1234].s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_mseg_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_misc_commands_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_mail_system_.[12].s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_fscom2_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_fscom1_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_exec_com_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_dm_.[123].s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_dial_out_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_absentee_com_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_video_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_vfile_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_tuning_.s.archive $S3/library_dir_dir/system_library_tools/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_ti_term_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_sort_routines_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_segment_info_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_probe_.[12].s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_printing_cmds_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_pnotice_.s.archive $S3/library_dir_dir/system_library_tools/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_mrds_.[1234].s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_misc_translatrs_.s.archive $S3/library_dir_dir/system_library_tools/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_menu_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_kermit_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_iodd_.s.archive $S3/library_dir_dir/system_library_tools/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_io_commands_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_fs_util_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_command_loop_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_command_env_.s.archive $S3/library_dir_dir/system_library_standard/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_as_misc_.s.archive $S3/library_dir_dir/system_library_tools/source/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_active_function_.s.archive $S3/library_dir_dir/system_library_standard/source/


mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_volume_retv_.archive $S3/library_dir_dir/system_library_tools/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_user_ctl_.archive $S3/library_dir_dir/system_library_tools/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_pnt_interface_.archive $S3/library_dir_dir/system_library_tools/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_io_tools_.archive $S3/library_dir_dir/system_library_tools/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_gm_.archive $S3/library_dir_dir/system_library_tools/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_dfm_.archive $S3/library_dir_dir/system_library_tools/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_copy_disk_vol_.archive $S3/library_dir_dir/system_library_tools/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_as_install_ctl_.archive $S3/library_dir_dir/system_library_tools/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_admin_tools_.archive $S3/library_dir_dir/system_library_tools/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_admin_rtnes_.archive $S3/library_dir_dir/system_library_tools/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_pl1_.[1234].archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_mseg_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_misc_commands_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_mail_system_.[12].archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_fscom2_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_fscom1_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_exec_com_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_dm_.[123].archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_dial_out_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_absentee_com_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_video_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_vfile_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_tuning_.archive $S3/library_dir_dir/system_library_tools/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_ti_term_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_sort_routines_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_segment_info_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_probe_.[12].archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_printing_cmds_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_pnotice_.archive $S3/library_dir_dir/system_library_tools/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_mrds_.[1234].archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_misc_translatrs_.archive $S3/library_dir_dir/system_library_tools/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_menu_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_kermit_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_iodd_.archive $S3/library_dir_dir/system_library_tools/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_io_commands_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_fs_util_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_command_loop_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_command_env_.archive $S3/library_dir_dir/system_library_standard/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_as_misc_.archive $S3/library_dir_dir/system_library_tools/object/
mv $S5/library_dir_dir/MR12.5/12.5.LDD/bound_active_function_.archive $S3/library_dir_dir/system_library_standard/object/

mv $S5/library_dir_dir/MR12.5/12.5.LDD/install_ldd.ec $S3/library_dir_dir/MR12.5/
rmdir $S5/library_dir_dir/MR12.5/12.5.LDD

mv $S5/library_dir_dir/MR12.5/12.5.LINUS/*.s.archive $S3/library_dir_dir/system_library_unbundled/source
mv $S5/library_dir_dir/MR12.5/12.5.LINUS/*.archive $S3/library_dir_dir/system_library_unbundled/object
mv $S5/library_dir_dir/MR12.5/12.5.LINUS/bound_linus_ $S3/system_library_unbundled
mv $S5/library_dir_dir/MR12.5/12.5.LINUS/install_linus.ec $S3/library_dir_dir/MR12.5/
rmdir $S5/library_dir_dir/MR12.5/12.5.LINUS

mv $S5/library_dir_dir/MR12.5/12.5.LISTER/*.s.archive $S3/library_dir_dir/system_library_unbundled/source
mv $S5/library_dir_dir/MR12.5/12.5.LISTER/*.archive $S3/library_dir_dir/system_library_unbundled/object
mv $S5/library_dir_dir/MR12.5/12.5.LISTER/bound_lister_ $S3/system_library_unbundled
mv $S5/library_dir_dir/MR12.5/12.5.LISTER/install_lister.ec $S3/library_dir_dir/MR12.5/
rmdir $S5/library_dir_dir/MR12.5/12.5.LISTER

mv $S5/library_dir_dir/MR12.5/12.5.PASCAL/*.s.archive $S3/library_dir_dir/system_library_unbundled/source
mv $S5/library_dir_dir/MR12.5/12.5.PASCAL/*.archive $S3/library_dir_dir/system_library_unbundled/object
mv $S5/library_dir_dir/MR12.5/12.5.PASCAL/bound_pascal_ $S3/system_library_unbundled
mv $S5/library_dir_dir/MR12.5/12.5.PASCAL/bound_pascal_runtime_ $S3/system_library_unbundled
mv $S5/library_dir_dir/MR12.5/12.5.PASCAL/install_pascal.ec $S3/library_dir_dir/MR12.5/
rmdir $S5/library_dir_dir/MR12.5/12.5.PASCAL

rmdir $S5/library_dir_dir/MR12.5
rmdir $S5/library_dir_dir
rmdir $S5

echo "Bulk extract"

# *.alm, *.pl1
(cd $S3 && find . -type f \( -name "*.alm" -o \
                             -name "*.pl1" -o \
                             -name "*.info" -o \
                             -name "*.cds" -o \
                             -name "*.ec" -o \
                             -name "*.header" -o \
                             -name "*.search" -o \
                             -name "*.bind" -o \
                             -name "*.bindmap" -o \
                             -name "*.bindmap" -o \
                             -name "*.mexp" -o \
                             -name "*.lisp" -o \
                             -name "*.bcpl" -o \
                             -name "*.bcpl" -o \
                             -name "*.pascal" -o \
                             -name "*.fortran" -o \
                             -name "*.checker*" -o \
                             -name "*.header" -o \
                             -name "*.search" -o \
                             -name "*.list*" -o \
                             -name "*.table" -o \
                             -name "*.message" -o \
                             -name "*.control" -o \
                             -name "*.teco" -o \
                             -name "*.ttf" -o \
                             -name "*.xdw" -o \
                             -name "*.pnotice" -o \
                             -name "*.rtmf" -o \
                             -name "*.cmf" -o \
                             -name "*.absin" -o \
                             -name "*.ssl" -o \
                             -name "*.ti" -o \
                             -name "*.pldt" -o \
                             -name "*.qedx" -o \
                             -name "*.src" -o \
                             -name "*.ct" -o \
                             -name "*.absin"-o  \
                             -name "*.compin" -o \
                             -name "*.compout" -o \
                             -name "*.dcl" -o \
                             -name "*.ge" -o \
                             -name "*.iodt" -o \
                             -name "*.ld" -o \
                             -name "*.listin" -o \
                             -name "*.bind_fnp" -o \
                             -name "tut_" \
 \)  -print0) | xargs -0 -n 1 -I{} ../tapeUtils/p72_to_acsii $S3/'{}' $T/'{}'

echo "Specials extract"

(cd $S3 && for N in documentation/MR12.5_SRB+SIB \
           documentation/error_msgs.compout/0 \
           documentation/error_msgs.compout/1 \
           documentation/facilities_data_dir/edoc_db \
           documentation/facilities_data_dir/online_doc.cmdb \
           documentation/subsystem/emacs/sample_start_up.emacs.lisp \
           documentation/MR12.3/error_msgs.compout/0 \
           documentation/MR12.3/error_msgs.compout/1 \
           documentation/MR12.3/SRB \
           documentation/MR12.3/SIB \
           documentation/MR12.3/TRs_fixed_in_MR12.3 \
           documentation/MR12.3/system_book_ \
           documentation/system_book_ \
           documentation/error_msgs.toc.compout \
           library_dir_dir/crossref/total.crossref/0 \
           library_dir_dir/crossref/total.crossref/1 \
           library_dir_dir/crossref/total.crossref/2 \
           library_dir_dir/crossref/total.crossref/3 \
           library_dir_dir/crossref/inst_dir/total.crossref/0 \
           library_dir_dir/crossref/inst_dir/total.crossref/1 \
           library_dir_dir/crossref/inst_dir/total.crossref/2 \
           library_dir_dir/crossref/inst_dir/total.crossref/3 \
           library_dir_dir/mcs/info/macros.asm \
           library_dir_dir/mcs/info/macros.map355 \
           library_dir_dir/system_library_1/info/hardcore_checker_map \
           library_dir_dir/system_library_tools/object/psp_info_ \
           library_dir_dir/system_library_tools/object/psp_info_.1 \
           ; do
echo $N
             ../../tapeUtils/p72_to_acsii $N ../$T/$N
           done)

echo "Archives extract"
# *.s.archive
(cd $S3 && find . -type f -name "*.s.archive" -print0) | xargs -0 -n 1 -I{} ../tapeUtils/p72archive_to_acsii $S3/'{}' $T/'{}'
(cd $S3 && find . -type f -name "*.incl.archive" -print0) | xargs -0 -n 1 -I{} ../tapeUtils/p72archive_to_acsii $S3/'{}' $T/'{}'
(cd $S3 && find . -type f -name "maps.archive" -print0) | xargs -0 -n 1 -I{} ../tapeUtils/p72archive_to_acsii $S3/'{}' $T/'{}'

# *.archive::*.bind
(cd $S3 && find . -type f -name "*.archive" -print0) | xargs -0 -n 1 -I{} ../tapeUtils/p72archive_bind_to_ascii $S3/'{}' $T/'{}'

echo "Notice"

cat > $T/README.txt <<NOTICE
                                          -----------------------------------------------------------


Historical Background

This edition of the Multics software materials and documentation is provided and donated
to Massachusetts Institute of Technology by Group BULL including BULL HN Information Systems Inc. 
as a contribution to computer science knowledge.  
This donation is made also to give evidence of the common contributions of Massachusetts Institute of Technology,
Bell Laboratories, General Electric, Honeywell Information Systems Inc., Honeywell BULL Inc., Groupe BULL
and BULL HN Information Systems Inc. to the development of this operating system. 
Multics development was initiated by Massachusetts Institute of Technology Project MAC (1963-1970),
renamed the MIT Laboratory for Computer Science and Artificial Intelligence in the mid 1970s, under the leadership
of Professor Fernando Jose Corbato. Users consider that Multics provided the best software architecture 
for managing computer hardware properly and for executing programs. Many subsequent operating systems 
incorporated Multics principles.
Multics was distributed in 1975 to 2000 by Group Bull in Europe , and in the U.S. by Bull HN Information Systems Inc., 
as successor in interest by change in name only to Honeywell Bull Inc. and Honeywell Information Systems Inc. .

                                          -----------------------------------------------------------

Permission to use, copy, modify, and distribute these programs and their documentation for any purpose and without
fee is hereby granted,provided that the below copyright notice and historical background appear in all copies
and that both the copyright notice and historical background and this permission notice appear in supporting
documentation, and that the names of MIT, HIS, BULL or BULL HN not be used in advertising or publicity pertaining
to distribution of the programs without specific prior written permission.
    Copyright 1972 by Massachusetts Institute of Technology and Honeywell Information Systems Inc.
    Copyright 2006 by BULL HN Information Systems Inc.
    Copyright 2006 by Bull SAS
    All Rights Reserved
NOTICE

echo "chmod ..."

chmod -w -R $T
