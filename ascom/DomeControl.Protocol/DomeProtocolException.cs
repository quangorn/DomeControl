using System;

namespace DomeControl.Protocol
{
    /// <summary>
    /// A response the firmware should not have sent: anything but <c>OK</c> where a command is
    /// acknowledged, a number where a number is expected, an out-of-range value for the encoder.
    /// The ASCOM driver turns this into an <c>ASCOM.DriverException</c>, so a client sees the same
    /// exception type as before this class existed (AGENTS.md §6).
    /// </summary>
    public class DomeProtocolException : Exception
    {
        public DomeProtocolException(string message) : base(message)
        {
        }

        public DomeProtocolException(string message, Exception innerException) : base(message, innerException)
        {
        }
    }
}