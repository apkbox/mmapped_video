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
    using System.Windows;
    using System.Windows.Threading;

    using Shadows.Modules.ServiceAndDiagnostics.ViewModels;

    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        #region Fields

        private readonly CameraVideoImageSource imageSource;

        #endregion

        #region Constructors and Destructors

        /// <summary>
        /// Initializes a new instance of the <see cref="MainWindow"/> class.
        /// </summary>
        public MainWindow()
        {
            this.InitializeComponent();

            this.imageSource = new CameraVideoImageSource(new DispatherSource(this.Dispatcher));
            this.imageSource.Open();
            this.Img.Source = this.imageSource.Source;
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
            this.imageSource.Close();
            base.OnClosed(e);
        }

        #endregion

        private class DispatherSource : IDispatcherSource
        {
            #region Fields

            private readonly Dispatcher dispatcher;

            #endregion

            #region Constructors and Destructors

            public DispatherSource(Dispatcher dispatcher)
            {
                this.dispatcher = dispatcher;
            }

            #endregion

            #region Public Methods and Operators

            public void Dispatch(Action action)
            {
                this.dispatcher.BeginInvoke(DispatcherPriority.Input, action);
            }

            #endregion
        }
    }
}
