// --------------------------------------------------------------------------------
// <copyright file="IDispatcherSource.cs" company="">
//   
// </copyright>
// <summary>
//   Defines the IDispatcherSource type.
// </summary>
// --------------------------------------------------------------------------------
namespace Shadows.Modules.ServiceAndDiagnostics.ViewModels
{
    using System;

    public interface IDispatcherSource
    {
        #region Public Methods and Operators

        void Dispatch(Action action);

        #endregion
    }
}
