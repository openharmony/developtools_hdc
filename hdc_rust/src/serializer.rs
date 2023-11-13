//! serializer

pub mod native_struct;
pub mod pack_assemble;
pub mod pack_struct;
pub mod serialize;
pub mod unittest;

pub use pack_assemble::{concat_pack, unpack_payload_head, unpack_payload_protect};
pub use pack_struct::{HEAD_SIZE, UART_HEAD_SIZE, USB_HEAD_SIZE};
pub use serialize::buf_to_vec;
