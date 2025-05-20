//
// Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
// SPDX-License-Identifier: AGPL-3.0-only
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "RuntimeError.h"
#include <openssl/err.h>


namespace Kourier
{

std::string_view RuntimeError::posixError(int error)
{
    switch (error)
    {
        case EPERM: return "POSIX error EPERM(1): Operation not permitted.";
        case ENOENT: return "POSIX error ENOENT(2): No such file or directory.";
        case ESRCH: return "POSIX error ESRCH(3): No such process.";
        case EINTR: return "POSIX error EINTR(4): Interrupted system call.";
        case EIO: return "POSIX error EIO(5): Input/output error.";
        case ENXIO: return "POSIX error ENXIO(6): No such device or address.";
        case E2BIG: return "POSIX error E2BIG(7): Argument list too long.";
        case ENOEXEC: return "POSIX error ENOEXEC(8): Exec format error.";
        case EBADF: return "POSIX error EBADF(9): Bad file descriptor.";
        case ECHILD: return "POSIX error ECHILD(10): No child processes.";
        case EAGAIN: return "POSIX error EAGAIN/EWOULDBLOCK(11): Resource temporarily unavailable.";
        case ENOMEM: return "POSIX error ENOMEM(12): Cannot allocate memory.";
        case EACCES: return "POSIX error EACCES(13): Permission denied.";
        case EFAULT: return "POSIX error EFAULT(14): Bad address.";
        case ENOTBLK: return "POSIX error ENOTBLK(15): Block device required.";
        case EBUSY: return "POSIX error EBUSY(16): Device or resource busy.";
        case EEXIST: return "POSIX error EEXIST(17): File exists.";
        case EXDEV: return "POSIX error EXDEV(18): Invalid cross-device link.";
        case ENODEV: return "POSIX error ENODEV(19): No such device.";
        case ENOTDIR: return "POSIX error ENOTDIR(20): Not a directory.";
        case EISDIR: return "POSIX error EISDIR(21): Is a directory.";
        case EINVAL: return "POSIX error EINVAL(22): Invalid argument.";
        case ENFILE: return "POSIX error ENFILE(23): Too many open files in system.";
        case EMFILE: return "POSIX error EMFILE(24): Too many open files.";
        case ENOTTY: return "POSIX error ENOTTY(25): Inappropriate ioctl for device.";
        case ETXTBSY: return "POSIX error ETXTBSY(26): Text file busy.";
        case EFBIG: return "POSIX error EFBIG(27): File too large.";
        case ENOSPC: return "POSIX error ENOSPC(28): No space left on device.";
        case ESPIPE: return "POSIX error ESPIPE(29): Illegal seek.";
        case EROFS: return "POSIX error EROFS(30): Read-only file system.";
        case EMLINK: return "POSIX error EMLINK(31): Too many links.";
        case EPIPE: return "POSIX error EPIPE(32): Broken pipe.";
        case EDOM: return "POSIX error EDOM(33): Numerical argument out of domain.";
        case ERANGE: return "POSIX error ERANGE(34): Numerical result out of range.";
        case EDEADLK: return "POSIX error EDEADLK/EDEADLOCK(35): Resource deadlock avoided.";
        case ENAMETOOLONG: return "POSIX error ENAMETOOLONG(36): File name too long.";
        case ENOLCK: return "POSIX error ENOLCK(37): No locks available.";
        case ENOSYS: return "POSIX error ENOSYS(38): Function not implemented.";
        case ENOTEMPTY: return "POSIX error ENOTEMPTY(39): Directory not empty.";
        case ELOOP: return "POSIX error ELOOP(40): Too many levels of symbolic links.";
        case ENOMSG: return "POSIX error ENOMSG(42): No message of desired type.";
        case EIDRM: return "POSIX error EIDRM(43): Identifier removed.";
        case ECHRNG: return "POSIX error ECHRNG(44): Channel number out of range.";
        case EL2NSYNC: return "POSIX error EL2NSYNC(45): Level 2 not synchronized.";
        case EL3HLT: return "POSIX error EL3HLT(46): Level 3 halted.";
        case EL3RST: return "POSIX error EL3RST(47): Level 3 reset.";
        case ELNRNG: return "POSIX error ELNRNG(48): Link number out of range.";
        case EUNATCH: return "POSIX error EUNATCH(49): Protocol driver not attached.";
        case ENOCSI: return "POSIX error ENOCSI(50): No CSI structure available.";
        case EL2HLT: return "POSIX error EL2HLT(51): Level 2 halted.";
        case EBADE: return "POSIX error EBADE(52): Invalid exchange.";
        case EBADR: return "POSIX error EBADR(53): Invalid request descriptor.";
        case EXFULL: return "POSIX error EXFULL(54): Exchange full.";
        case ENOANO: return "POSIX error ENOANO(55): No anode.";
        case EBADRQC: return "POSIX error EBADRQC(56): Invalid request code.";
        case EBADSLT: return "POSIX error EBADSLT(57): Invalid slot.";
        case EBFONT: return "POSIX error EBFONT(59): Bad font file format.";
        case ENOSTR: return "POSIX error ENOSTR(60): Device not a stream.";
        case ENODATA: return "POSIX error ENODATA(61): No data available.";
        case ETIME: return "POSIX error ETIME(62): Timer expired.";
        case ENOSR: return "POSIX error ENOSR(63): Out of streams resources.";
        case ENONET: return "POSIX error ENONET(64): Machine is not on the network.";
        case ENOPKG: return "POSIX error ENOPKG(65): Package not installed.";
        case EREMOTE: return "POSIX error EREMOTE(66): Object is remote.";
        case ENOLINK: return "POSIX error ENOLINK(67): Link has been severed.";
        case EADV: return "POSIX error EADV(68): Advertise error.";
        case ESRMNT: return "POSIX error ESRMNT(69): Srmount error.";
        case ECOMM: return "POSIX error ECOMM(70): Communication error on send.";
        case EPROTO: return "POSIX error EPROTO(71): Protocol error.";
        case EMULTIHOP: return "POSIX error EMULTIHOP(72): Multihop attempted.";
        case EDOTDOT: return "POSIX error EDOTDOT(73): RFS specific error.";
        case EBADMSG: return "POSIX error EBADMSG(74): Bad message.";
        case EOVERFLOW: return "POSIX error EOVERFLOW(75): Value too large for defined data type.";
        case ENOTUNIQ: return "POSIX error ENOTUNIQ(76): Name not unique on network.";
        case EBADFD: return "POSIX error EBADFD(77): File descriptor in bad state.";
        case EREMCHG: return "POSIX error EREMCHG(78): Remote address changed.";
        case ELIBACC: return "POSIX error ELIBACC(79): Can not access a needed shared library.";
        case ELIBBAD: return "POSIX error ELIBBAD(80): Accessing a corrupted shared library.";
        case ELIBSCN: return "POSIX error ELIBSCN(81): .lib section in a.out corrupted.";
        case ELIBMAX: return "POSIX error ELIBMAX(82): Attempting to link in too many shared libraries.";
        case ELIBEXEC: return "POSIX error ELIBEXEC(83): Cannot exec a shared library directly.";
        case EILSEQ: return "POSIX error EILSEQ(84): Invalid or incomplete multibyte or wide character.";
        case ERESTART: return "POSIX error ERESTART(85): Interrupted system call should be restarted.";
        case ESTRPIPE: return "POSIX error ESTRPIPE(86): Streams pipe error.";
        case EUSERS: return "POSIX error EUSERS(87): Too many users.";
        case ENOTSOCK: return "POSIX error ENOTSOCK(88): Socket operation on non-socket.";
        case EDESTADDRREQ: return "POSIX error EDESTADDRREQ(89): Destination address required.";
        case EMSGSIZE: return "POSIX error EMSGSIZE(90): Message too long.";
        case EPROTOTYPE: return "POSIX error EPROTOTYPE(91): Protocol wrong type for socket.";
        case ENOPROTOOPT: return "POSIX error ENOPROTOOPT(92): Protocol not available.";
        case EPROTONOSUPPORT: return "POSIX error EPROTONOSUPPORT(93): Protocol not supported.";
        case ESOCKTNOSUPPORT: return "POSIX error ESOCKTNOSUPPORT(94): Socket type not supported.";
        case EOPNOTSUPP: return "POSIX error EOPNOTSUPP/ENOTSUP(95): Operation not supported.";
        case EPFNOSUPPORT: return "POSIX error EPFNOSUPPORT(96): Protocol family not supported.";
        case EAFNOSUPPORT: return "POSIX error EAFNOSUPPORT(97): Address family not supported by protocol.";
        case EADDRINUSE: return "POSIX error EADDRINUSE(98): Address already in use.";
        case EADDRNOTAVAIL: return "POSIX error EADDRNOTAVAIL(99): Cannot assign requested address.";
        case ENETDOWN: return "POSIX error ENETDOWN(100): Network is down.";
        case ENETUNREACH: return "POSIX error ENETUNREACH(101): Network is unreachable.";
        case ENETRESET: return "POSIX error ENETRESET(102): Network dropped connection on reset.";
        case ECONNABORTED: return "POSIX error ECONNABORTED(103): Software caused connection abort.";
        case ECONNRESET: return "POSIX error ECONNRESET(104): Connection reset by peer.";
        case ENOBUFS: return "POSIX error ENOBUFS(105): No buffer space available.";
        case EISCONN: return "POSIX error EISCONN(106): Transport endpoint is already connected.";
        case ENOTCONN: return "POSIX error ENOTCONN(107): Transport endpoint is not connected.";
        case ESHUTDOWN: return "POSIX error ESHUTDOWN(108): Cannot send after transport endpoint shutdown.";
        case ETOOMANYREFS: return "POSIX error ETOOMANYREFS(109): Too many references: cannot splice.";
        case ETIMEDOUT: return "POSIX error ETIMEDOUT(110): Connection timed out.";
        case ECONNREFUSED: return "POSIX error ECONNREFUSED(111): Connection refused.";
        case EHOSTDOWN: return "POSIX error EHOSTDOWN(112): Host is down.";
        case EHOSTUNREACH: return "POSIX error EHOSTUNREACH(113): No route to host.";
        case EALREADY: return "POSIX error EALREADY(114): Operation already in progress.";
        case EINPROGRESS: return "POSIX error EINPROGRESS(115): Operation now in progress.";
        case ESTALE: return "POSIX error ESTALE(116): Stale file handle.";
        case EUCLEAN: return "POSIX error EUCLEAN(117): Structure needs cleaning.";
        case ENOTNAM: return "POSIX error ENOTNAM(118): Not a XENIX named type file.";
        case ENAVAIL: return "POSIX error ENAVAIL(119): No XENIX semaphores available.";
        case EISNAM: return "POSIX error EISNAM(120): Is a named type file.";
        case EREMOTEIO: return "POSIX error EREMOTEIO(121): Remote I/O error.";
        case EDQUOT: return "POSIX error EDQUOT(122): Disk quota exceeded.";
        case ENOMEDIUM: return "POSIX error ENOMEDIUM(123): No medium found.";
        case EMEDIUMTYPE: return "POSIX error EMEDIUMTYPE(124): Wrong medium type.";
        case ECANCELED: return "POSIX error ECANCELED(125): Operation canceled.";
        case ENOKEY: return "POSIX error ENOKEY(126): Required key not available.";
        case EKEYEXPIRED: return "POSIX error EKEYEXPIRED(127): Key has expired.";
        case EKEYREVOKED: return "POSIX error EKEYREVOKED(128): Key has been revoked.";
        case EKEYREJECTED: return "POSIX error EKEYREJECTED(129): Key was rejected by service.";
        case EOWNERDEAD: return "POSIX error EOWNERDEAD(130): Owner died.";
        case ENOTRECOVERABLE: return "POSIX error ENOTRECOVERABLE(131): State not recoverable.";
        case ERFKILL: return "POSIX error ERFKILL(132): Operation not possible due to RF-kill.";
        case EHWPOISON: return "POSIX error EHWPOISON(133): Memory page has hardware error.";
        case 0: return "";
        default: return "Unrecognized POSIX error number.";
    }
}

RuntimeError::RuntimeError(std::string_view errorMessage, ErrorType errorType)
{
    m_errorMessage.append(errorMessage);
    switch (errorType)
    {
        case ErrorType::User:
            return;
        case ErrorType::POSIX:
            m_errorMessage.append(" ").append(posixError(errno));
            return;
        case ErrorType::TLS:
        {
            m_errorMessage.append(" TLS error");
            unsigned long errorCode = ERR_get_error();
            while (ERR_get_error() != 0) {}
            const char * pLibErrorString = ERR_lib_error_string(errorCode);
            if (pLibErrorString)
                m_errorMessage.append(" at ").append(pLibErrorString).append(".");
            const char * pReasonErrorString = ERR_reason_error_string(errorCode);
            if (pReasonErrorString)
                m_errorMessage.append(" ").append(pReasonErrorString);
            break;
        }
    }
}

}
