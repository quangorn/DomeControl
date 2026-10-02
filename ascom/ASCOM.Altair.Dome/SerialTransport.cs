using ASCOM.Utilities;
using DomeControl.Protocol;

namespace ASCOM.Altair
{
    /// <summary>
    /// Presents the open COM port to <see cref="DomeProtocol"/> as an <see cref="IDomeTransport"/>.
    ///
    /// This is the only file in the driver that knows about ASCOM.Utilities.Serial; the protocol
    /// itself takes the three operations it needs, which is what lets the same code run against a
    /// socket to the simulator in the tests (AGENTS.md §6).
    ///
    /// The adapter does not own the port: Driver.cs creates and closes the <see cref="Serial"/>, so
    /// Dispose here only breaks the reference.
    /// </summary>
    internal sealed class SerialTransport : IDomeTransport
    {
        private readonly Serial serialPort;

        internal SerialTransport(Serial serialPort)
        {
            this.serialPort = serialPort;
        }

        public void ClearBuffers()
        {
            serialPort.ClearBuffers();
        }

        public void Transmit(string frame)
        {
            serialPort.Transmit(frame);
        }

        public string ReceiveTerminated(string terminator)
        {
            return serialPort.ReceiveTerminated(terminator);
        }

        public void Dispose()
        {
            // The port belongs to the driver; closing it here would hide the trace log message
            // Driver.cs writes when the port refuses to close.
        }
    }
}