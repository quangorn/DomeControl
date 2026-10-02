using System;

namespace DomeControl.Protocol
{
    /// <summary>
    /// The serial link, as the protocol layer needs it: three operations and nothing else. The
    /// ASCOM driver adapts <c>ASCOM.Utilities.Serial</c> to this; the tests use a loopback or a
    /// socket to the simulator. Keeping ASCOM out of this interface is what lets the protocol be
    /// tested without Windows (AGENTS.md §6).
    /// </summary>
    public interface IDomeTransport : IDisposable
    {
        /// <summary>Drops whatever is queued in both directions.</summary>
        void ClearBuffers();

        /// <summary>Sends a whole command frame, terminator included.</summary>
        void Transmit(string frame);

        /// <summary>Reads until <paramref name="terminator"/> and returns the text up to and including it.</summary>
        string ReceiveTerminated(string terminator);
    }
}