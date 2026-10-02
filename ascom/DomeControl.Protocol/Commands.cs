namespace DomeControl.Protocol
{
    /// <summary>
    /// Command strings, a mirror of firmware/source/common/definitions.c.
    /// tools/test_protocol_contract.py keeps the two in step; change one, change the other.
    /// </summary>
    public static class Commands
    {
        public const string GO_FORWARD = "GOF";
        public const string GO_REVERSE = "GOR";
        public const string STOP = "ST";
        public const string GOTO = "GT";
        public const string GET_ENCODER_VALUE = "GEV";
        public const string IS_ON_CENTER = "IOC";
        public const string IS_MOVING = "IM";
        public const string FIND_CENTER = "FC";
    }
}