using System;

namespace DomeControl.Protocol
{
    /// <summary>
    /// The USART contract with the firmware (AGENTS.md §5): frame construction, response parsing
    /// and the azimuth conversion. There is no ASCOM type in this file and none in this assembly,
    /// which is what lets it be built and tested on Linux next to the firmware.
    ///
    /// Every command goes through one lock. A client polls <c>Slewing</c> while another thread
    /// issues <c>AbortSlew</c>, the firmware has a single command buffer and no queue, and
    /// <see cref="IDomeTransport.ClearBuffers"/> drops whatever the other thread has not read yet,
    /// so overlapping transactions corrupt each other.
    /// </summary>
    public sealed class DomeProtocol
    {
        /// <summary>End of a command frame; mirrors END_COMMAND_CHARACTER in the firmware.</summary>
        public const char EndCommandCharacter = '#';

        /// <summary>Azimuth the encoder reads 0 at.</summary>
        public const double HomePositionAzimuth = 180;

        /// <summary>Degrees per encoder step: 360 degrees over 675 teeth.</summary>
        public const double EncoderStepAzimuthDegrees = 0.533;

        private readonly IDomeTransport transport;
        private readonly object gate = new object();

        public DomeProtocol(IDomeTransport transport)
        {
            if (transport == null)
            {
                throw new ArgumentNullException(nameof(transport));
            }
            this.transport = transport;
        }

        /// <summary>
        /// Sends one command and returns the firmware's reply, terminator included.
        /// <paramref name="raw"/> means the caller supplies the terminator itself.
        /// </summary>
        public string CommandString(string command, bool raw)
        {
            lock (gate)
            {
                string frame = raw ? command : command + EndCommandCharacter;
                transport.ClearBuffers();
                transport.Transmit(frame);
                return transport.ReceiveTerminated("\n");
            }
        }

        /// <summary>Issues a command the firmware acknowledges with <c>OK</c>.</summary>
        public void SendCommandWithSimpleResp(string command)
        {
            string response = CommandString(command, false).Trim();
            if (!response.Equals(Responses.OK))
            {
                throw new DomeProtocolException(
                    string.Format("Bad command {0} response: {1}", command, response));
            }
        }

        /// <summary>Issues a command the firmware answers with a decimal encoder value.</summary>
        public short SendCommandWithIntResp(string command)
        {
            string response = CommandString(command, false);
            try
            {
                return short.Parse(response);
            }
            catch (Exception e)
            {
                if (e is FormatException || e is OverflowException)
                {
                    throw new DomeProtocolException(
                        string.Format("Bad command {0} int response: {1}", command, response), e);
                }
                throw;
            }
        }

        /// <summary>Issues a command the firmware answers with 0 or 1.</summary>
        public bool SendCommandWithBoolResp(string command)
        {
            return SendCommandWithIntResp(command) != 0;
        }

        public void GoForward()
        {
            SendCommandWithSimpleResp(Commands.GO_FORWARD);
        }

        public void GoReverse()
        {
            SendCommandWithSimpleResp(Commands.GO_REVERSE);
        }

        public void MotorStop()
        {
            SendCommandWithSimpleResp(Commands.STOP);
        }

        public void FindCenter()
        {
            SendCommandWithSimpleResp(Commands.FIND_CENTER);
        }

        /// <summary>Slews to an encoder position. The firmware clamps it to the travelled range.</summary>
        public void GoTo(short targetEncoderPosition)
        {
            SendCommandWithSimpleResp(string.Format("{0}{1}", Commands.GOTO, targetEncoderPosition));
        }

        public short GetEncoderValue()
        {
            return SendCommandWithIntResp(Commands.GET_ENCODER_VALUE);
        }

        public bool IsOnCenter()
        {
            return SendCommandWithBoolResp(Commands.IS_ON_CENTER);
        }

        public bool IsMoving()
        {
            return SendCommandWithBoolResp(Commands.IS_MOVING);
        }

        /// <summary>
        /// Turns an encoder value into an azimuth in [0, 360). The encoder is relative to the last
        /// limit switch that fired, so this is only an absolute azimuth after <c>FC</c> or a limit
        /// (AGENTS.md §8).
        /// </summary>
        public double EncoderValueToAzimuth(short encoderValue)
        {
            double azimuth = HomePositionAzimuth + EncoderStepAzimuthDegrees * encoderValue;
            // Normalise into [0, 360): the dome range is currently +-100 steps, so the lower branch
            // never fires today, but a wider range would return values above 360 to the client.
            azimuth %= 360.0;
            if (azimuth < 0)
            {
                azimuth += 360.0;
            }
            return azimuth;
        }

        /// <summary>
        /// Turns a requested azimuth into an encoder position, taking the short way round: a delta
        /// above 180 degrees means -360, because the dome cannot spin a full turn.
        /// </summary>
        public short AzimuthToEncoderValue(double azimuth)
        {
            double azimuthDelta = azimuth - HomePositionAzimuth;
            if (azimuthDelta > 180)
            {
                azimuthDelta -= 360;
            }
            return (short)Math.Round(azimuthDelta / EncoderStepAzimuthDegrees);
        }
    }
}