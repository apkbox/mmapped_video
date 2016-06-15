// --------------------------------------------------------------------------------
// <copyright file="MainWindow.xaml.cs" company="">
//   
// </copyright>
// <summary>
//   The BITMAP structure defines the type, width, height, color format, and bit values of a bitmap.
// </summary>
// --------------------------------------------------------------------------------

namespace mmap_client
{
    using System;
    using System.Diagnostics;
    using System.IO.MemoryMappedFiles;
    using System.Runtime.InteropServices;
    using System.Threading;
    using System.Windows;
    using System.Windows.Interop;
    using System.Windows.Media;
    using System.Windows.Threading;

    /// <summary>
    /// The BITMAP structure defines the type, width, height, color format, and bit values of a bitmap.
    /// </summary>
    [Serializable]
    [StructLayout(LayoutKind.Sequential)]
    public struct BITMAP
    {
        /// <summary>
        /// The bitmap type. This member must be zero.
        /// </summary>
        public int bmType;

        /// <summary>
        /// The width, in pixels, of the bitmap. The width must be greater than zero.
        /// </summary>
        public int bmWidth;

        /// <summary>
        /// The height, in pixels, of the bitmap. The height must be greater than zero.
        /// </summary>
        public int bmHeight;

        /// <summary>
        /// The number of bytes in each scan line. This value must be divisible by 2, because the system assumes that the bit 
        /// values of a bitmap form an array that is word aligned.
        /// </summary>
        public int bmWidthBytes;

        /// <summary>
        /// The count of color planes.
        /// </summary>
        public int bmPlanes;

        /// <summary>
        /// The number of bits required to indicate the color of a pixel.
        /// </summary>
        public int bmBitsPixel;

        /// <summary>
        /// A pointer to the location of the bit values for the bitmap. The bmBits member must be a pointer to an array of 
        /// character (1-byte) values.
        /// </summary>
        public IntPtr bmBits;
    }

    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        #region Fields

        /// <summary>
        /// The bitmap_data.
        /// </summary>
        private byte[] bitmap_data;

        /// <summary>
        /// The bmpsrc.
        /// </summary>
        private InteropBitmap bmpsrc;

        /// <summary>
        /// The event_.
        /// </summary>
        private EventWaitHandle event_;

        /// <summary>
        /// The mmap.
        /// </summary>
        private MemoryMappedFile mmap;

        /// <summary>
        /// The thread.
        /// </summary>
        private Thread thread;

        /// <summary>
        /// The view.
        /// </summary>
        private MemoryMappedViewAccessor view;

        #endregion

        #region Constructors and Destructors

        /// <summary>
        /// Initializes a new instance of the <see cref="MainWindow"/> class.
        /// </summary>
        public MainWindow()
        {
            this.InitializeComponent();

            this.event_ = EventWaitHandle.OpenExisting("ArizonaVisionSystemVideoReadyEvent");
            this.mmap = MemoryMappedFile.OpenExisting("ArizonaVisionSystemVideoBitmap", MemoryMappedFileRights.Read);
            this.view = this.mmap.CreateViewAccessor(0, 0, MemoryMappedFileAccess.Read);
            BITMAP bmp;
            this.view.Read(0, out bmp);
            var width = bmp.bmWidth;
            var height = bmp.bmHeight;
            var pixelFormat = PixelFormats.Gray8;
            var bytesPerPixel = (pixelFormat.BitsPerPixel + 7) / 8;
            var stride = bytesPerPixel * width;

            this.bmpsrc =
                Imaging.CreateBitmapSourceFromMemorySection(
                    this.mmap.SafeMemoryMappedFileHandle.DangerousGetHandle(), 
                    width, 
                    height, 
                    pixelFormat, 
                    stride, 
                    bmp.bmBits.ToInt32()) as InteropBitmap;

            this.Img.Source = this.bmpsrc;

            this.thread = new Thread(
                o =>
                    {
                        try
                        {
                            while (this.view != null)
                            {
                                this.event_.WaitOne(100);
                                this.Dispatcher.BeginInvoke(
                                    DispatcherPriority.Input, 
                                    new Action(
                                        () =>
                                            {
                                                if (this.bmpsrc != null)
                                                {
                                                    this.bmpsrc.Invalidate();
                                                }
                                            }));
                            }
                        }
                        catch (Exception e)
                        {
                            Debug.Fail(e.ToString());
                        }
                    });
            this.thread.Start();
        }

        #endregion

        #region Methods

        /// <summary>
        /// The on closed.
        /// </summary>
        /// <param name="e">
        /// The e.
        /// </param>
        protected override void OnClosed(EventArgs e)
        {
            base.OnClosed(e);
            this.view = null;
            this.event_.Set();
            this.thread.Join();
        }

        #endregion
    }
}