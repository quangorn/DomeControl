using System;
using System.Diagnostics;
using System.IO;
using System.Net.Sockets;
using System.Text;
using DomeControl.Protocol;

namespace DomeControl.Protocol.Tests
{
    /// <summary>
    /// The firmware ELF running under simavr, spoken to over the harness control channel (stdin and
    /// stdout, one command per line). The harness announces its bridge with `uart &lt;kind&gt; &lt;where&gt;`
    /// once it is ready.
    /// </summary>
    internal sealed class SimulatorProcess : IDisposable
    {
        private readonly Process process;

        private SimulatorProcess(Process process, int port)
        {
            this.process = process;
            Port = port;
        }

        /// <summary>The TCP port the firmware's USART is bridged to.</summary>
        public int Port { get; }

        public static SimulatorProcess Start(string harnessPath, string firmwarePath)
        {
            var process = new Process
            {
                StartInfo = new ProcessStartInfo(harnessPath, "--tcp " + firmwarePath)
                {
                    RedirectStandardInput = true,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false,
                },
            };
            process.Start();
            try
            {
                while (true)
                {
                    string line = process.StandardOutput.ReadLine();
                    if (line == null)
                    {
                        throw new InvalidOperationException("the harness exited before announcing a bridge");
                    }
                    int marker = line.IndexOf('@');
                    if (marker < 0)
                    {
                        continue;           // simavr's own output shares stdout
                    }
                    string reply = line.Substring(marker + 1).Trim();
                    if (reply.StartsWith("uart tcp ", StringComparison.Ordinal))
                    {
                        return new SimulatorProcess(process, int.Parse(reply.Substring("uart tcp ".Length)));
                    }
                }
            }
            catch
            {
                Kill(process);
                throw;
            }
        }

        /// <summary>Sends one control command and returns its reply line.</summary>
        public string Command(string text)
        {
            process.StandardInput.WriteLine(text);
            process.StandardInput.Flush();
            while (true)
            {
                string line = process.StandardOutput.ReadLine();
                if (line == null)
                {
                    throw new InvalidOperationException("the harness exited while handling '" + text + "'");
                }
                int marker = line.IndexOf('@');
                if (marker >= 0)
                {
                    return line.Substring(marker + 1).Trim();
                }
            }
        }

        /// <summary>Advances simulated time and releases every switch pin, so nothing reads as pressed.</summary>
        public void Settle()
        {
            foreach (string pin in new[] { "C 0", "C 1", "C 2", "D 3", "D 4" })
            {
                Command("set " + pin + " 1");
            }
            Command("wait 200");
        }

        public void Dispose()
        {
            try
            {
                process.StandardInput.WriteLine("quit");
                process.StandardInput.Flush();
                if (!process.WaitForExit(2000))
                {
                    Kill(process);
                }
            }
            catch (Exception)
            {
                Kill(process);
            }
        }

        private static void Kill(Process process)
        {
            try
            {
                if (!process.HasExited)
                {
                    process.Kill();
                    process.WaitForExit(2000);
                }
            }
            catch (Exception)
            {
                // the process is already gone; nothing left to clean up
            }
        }
    }

    /// <summary>
    /// The same USART the firmware sees, reached over TCP instead of a COM port. This is what lets
    /// the driver's own protocol code be exercised against the real firmware on Linux.
    /// </summary>
    internal sealed class TcpTransport : IDomeTransport
    {
        private readonly TcpClient client;
        private readonly NetworkStream stream;
        private readonly byte[] buffer = new byte[1];

        public TcpTransport(int port)
        {
            client = new TcpClient();
            client.Connect("127.0.0.1", port);
            client.NoDelay = true;
            stream = client.GetStream();
            stream.ReadTimeout = 5000;
            stream.WriteTimeout = 5000;
        }

        public void ClearBuffers()
        {
            // Nothing may survive from a previous transaction: the firmware has one command buffer,
            // and a stale byte would be dispatched as the next command.
            stream.ReadTimeout = 50;
            try
            {
                while (stream.Read(buffer, 0, 1) > 0)
                {
                }
            }
            catch (IOException)
            {
                // a read timeout is the normal way out here
            }
            finally
            {
                stream.ReadTimeout = 5000;
            }
        }

        public void Transmit(string frame)
        {
            byte[] bytes = Encoding.ASCII.GetBytes(frame);
            stream.Write(bytes, 0, bytes.Length);
            stream.Flush();
        }

        public string ReceiveTerminated(string terminator)
        {
            char stop = terminator[0];
            var text = new StringBuilder();
            while (true)
            {
                int count = stream.Read(buffer, 0, 1);
                if (count <= 0)
                {
                    throw new InvalidOperationException("the firmware closed the link");
                }
                char character = (char)buffer[0];
                text.Append(character);
                if (character == stop)
                {
                    return text.ToString();
                }
            }
        }

        public void Dispose()
        {
            client.Dispose();
        }
    }
}