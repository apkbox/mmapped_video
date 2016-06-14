using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace mmap_client
{
    using System.IO.MemoryMappedFiles;
    using System.Runtime.InteropServices;
    using System.Threading;
    using System.Windows.Threading;

    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();

            this.event_ = EventWaitHandle.OpenExisting("mmap_video_event");
            this.mmap = MemoryMappedFile.OpenExisting("mmap_video_data", MemoryMappedFileRights.Read);
            view = mmap.CreateViewAccessor(0, 0, MemoryMappedFileAccess.Read);
            var width =  view.ReadInt32(0);
            var height = view.ReadInt32(4);
            var pixelFormat = PixelFormats.Gray8;
            var bytesPerPixel = (pixelFormat.BitsPerPixel + 7) / 8;
            var stride = bytesPerPixel * width;

            this.bitmap_data = new byte[width * height];
            this.bmpsrc = new WriteableBitmap(width, height, 96, 96, pixelFormat, null);
            this.Img.Source = this.bmpsrc;

            this.thread = new Thread(
                o =>
                    {
                        try
                        {
                            while (view != null)
                            {
                                this.event_.WaitOne(100);
                                view.ReadArray(0, bitmap_data, 0, bitmap_data.Length);
                                this.Dispatcher.BeginInvoke(DispatcherPriority.Input,
                                    new Action(
                                        () =>
                                            {
                                                Int32Rect r = new Int32Rect(0, 0, width, height);
                                                bmpsrc.WritePixels(r, bitmap_data, width, 0);
                                                bmpsrc.Lock();
                                                bmpsrc.AddDirtyRect(r);
                                                bmpsrc.Unlock();
                                            }));
                            }
                        }
                        catch (Exception e)
                        {
                            
                        }
                    });
            this.thread.Start();
        }

        protected override void OnClosed(EventArgs e)
        {
            base.OnClosed(e);
            view = null;
            this.event_.Set();
            this.thread.Join();
        }

        private MemoryMappedFile mmap;

        private MemoryMappedViewAccessor view;

        private WriteableBitmap bmpsrc;

        private EventWaitHandle event_;

        private Thread thread;

        private byte[] bitmap_data;
    }
}
