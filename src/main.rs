use nix::libc::MS_NOSUID;
use nix::mount::mount;
use nix::sched::clone;
use nix::sched::CloneFlags;
use nix::sys::signal::Signal::SIGCHLD;
use nix::sys::wait::waitpid;
use nix::unistd::execv;
use nix::mount::MsFlags;
use std::thread;
use std::time::Duration;
use std::ffi::CStr;
use std::ffi::CString;

const STACK_SIZE: usize = 4 * 1024 * 1024;

fn child_process() -> isize {
    let args = [
        CString::new("/bin/bash").unwrap(), CString::new("-i").unwrap()
    ];

    // Initiate the container by mount proc to /proc
    let mount_flags = MsFlags::MS_NOEXEC | MsFlags::MS_NODEV | MsFlags::MS_NOSUID;
    mount::<str, str, str, str>(Some("proc"), "/proc", Some("proc"), mount_flags, None).unwrap();
    execv(&args[0], &args).unwrap();
    0
}

fn main() {
    let clone_flags = CloneFlags::CLONE_NEWUTS | CloneFlags::CLONE_NEWPID | CloneFlags::CLONE_NEWNS;
    let ref mut stack: [u8; STACK_SIZE] = [0; STACK_SIZE];

    let pid = clone(
        Box::new(child_process),
        stack,
        clone_flags,
        Some(SIGCHLD as i32),
    )
    .unwrap();
    waitpid(pid, None).unwrap();
}
