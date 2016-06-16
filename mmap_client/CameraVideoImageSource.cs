// --------------------------------------------------------------------------------
// <copyright file="CameraVideoImageSource.cs" company="Oce Display Graphics Systems, Inc.">
//   Copyright (c) Oce Display Graphics Systems, Inc. All rights reserved.
// </copyright>
// <summary>
//   Defines the CameraVideoImageSource type.
// </summary>
// --------------------------------------------------------------------------------
namespace Shadows.Modules.ServiceAndDiagnostics.ViewModels
{
    using System;
    using System.Diagnostics;
    using System.IO.MemoryMappedFiles;
    using System.Threading;
    using System.Windows.Interop;
    using System.Windows.Media;

    public class CameraVideoImageSource : IDisposable
    {
        #region Constants

        private const string ReadyEventName = "ArizonaVisionSystemVideoReadyEvent";

        private const string SharedMemoryName = "ArizonaVisionSystemVideoBitmap";

        #endregion

        #region Fields

        private readonly IDispatcherSource dispatcherSource;

        private bool disposed;

        private EventWaitHandle readyEvent;

        private MemoryMappedFile sharedMemory;

        private InteropBitmap source;

        private Thread refreshThread;

        private MemoryMappedViewAccessor view;

        #endregion

        #region Constructors and Destructors

        public CameraVideoImageSource(IDispatcherSource dispatcherSource)
        {
            this.dispatcherSource = dispatcherSource;
        }

        ~CameraVideoImageSource()
        {
            this.Dispose(false);
        }

        #endregion

        #region Public Properties

        public ImageSource Source
        {
            get
            {
                return this.source;
            }
        }

        #endregion

        #region Public Methods and Operators

        public void Close()
        {
            this.Cleanup();
        }

        public void Dispose()
        {
            this.Dispose(true);
            System.GC.SuppressFinalize(this);
        }

        public bool Open()
        {
            if (this.view != null)
            {
                return false;
            }

            try
            {
                this.readyEvent = EventWaitHandle.OpenExisting(ReadyEventName);
                this.sharedMemory = MemoryMappedFile.OpenExisting(SharedMemoryName, MemoryMappedFileRights.Read);
                this.view = this.sharedMemory.CreateViewAccessor(0, 0, MemoryMappedFileAccess.Read);
            }
            catch (Exception e)
            {
                Debug.WriteLine(e.ToString());
                this.Cleanup();
                return false;
            }

            BITMAP bmp;
            this.view.Read(0, out bmp);
            var width = bmp.bmWidth;
            var height = bmp.bmHeight;
            var pixelFormat = PixelFormats.Gray8;
            var bytesPerPixel = (pixelFormat.BitsPerPixel + 7) / 8;
            var stride = bytesPerPixel * width;

            var shmemPtr = this.sharedMemory.SafeMemoryMappedFileHandle.DangerousGetHandle();
            this.source = Imaging.CreateBitmapSourceFromMemorySection(
                shmemPtr, 
                width, 
                height, 
                pixelFormat, 
                stride, 
                bmp.bmBits.ToInt32()) as InteropBitmap;
            if (this.source == null)
            {
                const string ErrorMessage = "Unexpected type from CreateBitmapSourceFromMemorySection.";
                Debug.Fail(ErrorMessage);
                Debug.WriteLine(ErrorMessage);
                this.Cleanup();
                return false;
            }

            this.refreshThread = new Thread(this.RefreshThread);
            this.refreshThread.Start();
            return true;
        }

        #endregion

        #region Methods

        protected virtual void Dispose(bool disposing)
        {
            if (!this.disposed)
            {
                if (disposing)
                {
                    this.Cleanup();
                }

                this.disposed = true;
            }
        }

        private void Cleanup()
        {
            this.source = null;
            if (this.refreshThread != null)
            {
                this.refreshThread.Join();
                this.refreshThread = null;
            }

            if (this.readyEvent != null)
            {
                this.readyEvent.Dispose();
                this.readyEvent = null;
            }

            if (this.view != null)
            {
                this.view.Dispose();
                this.view = null;
            }

            if (this.sharedMemory != null)
            {
                this.sharedMemory.Dispose();
                this.sharedMemory = null;
            }
        }

        private void RefreshSource()
        {
            if (this.source != null)
            {
                this.source.Invalidate();
            }
        }

        private void RefreshThread(object o)
        {
            try
            {
                while (this.source != null)
                {
                    this.readyEvent.WaitOne(500);
                    this.dispatcherSource.Dispatch(this.RefreshSource);
                }
            }
            catch (Exception e)
            {
                Debug.Fail(e.ToString());
            }
        }

        #endregion
    }
}
