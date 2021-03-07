using System;
using System.Timers;
using System.Windows.Forms;

namespace ASCOM.Altair
{
    public partial class Form1 : Form
    {

        private ASCOM.DriverAccess.Dome driver;

        public Form1()
        {
            InitializeComponent();
            SetUIState();
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (IsConnected)
                driver.Connected = false;

            Properties.Settings.Default.Save();
        }

        private void buttonChoose_Click(object sender, EventArgs e)
        {
            Properties.Settings.Default.DriverId = ASCOM.DriverAccess.Dome.Choose(Properties.Settings.Default.DriverId);
            SetUIState();
        }

        private void buttonConnect_Click(object sender, EventArgs e)
        {
            if (IsConnected)
            {
                driver.Connected = false;
            }
            else
            {
                driver = new DriverAccess.Dome(Properties.Settings.Default.DriverId);
                driver.Connected = true;
            }
            SetUIState();
        }

        private void buttonFindHome_Click(object sender, EventArgs e)
        {
            if (IsConnected)
            {
                driver.FindHome();
            }
            SetUIState();
        }

        private void updateTimer_Elapsed(object sender, ElapsedEventArgs e)
        {
            SetUIState();
        }

        private void SetUIState()
        {
            buttonConnect.Enabled = !string.IsNullOrEmpty(Properties.Settings.Default.DriverId);
            buttonChoose.Enabled = !IsConnected;
            buttonConnect.Text = IsConnected ? "Disconnect" : "Connect";
            if (driver != null && driver.Connected)
            {
                labelAzimuth.Text = driver.Azimuth.ToString();
            }
            else
            {
                labelAzimuth.Text = "";
            }
        }

        private bool IsConnected
        {
            get
            {
                return ((this.driver != null) && (driver.Connected == true));
            }
        }
    }
}
