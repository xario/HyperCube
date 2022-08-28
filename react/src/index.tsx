import * as React from 'react';
import ReactDOM from 'react-dom';
import CssBaseline from '@mui/material/CssBaseline';
import { ThemeProvider } from '@mui/material/styles';
import App from './App';
import theme from './theme';
import { SnackbarProvider } from "notistack";
import Fade from '@mui/material/Fade';
import type {ComponentType} from 'react';

let transition: ComponentType = Fade;
ReactDOM.render(
  <ThemeProvider theme={theme}>
    <CssBaseline />
    <SnackbarProvider maxSnack={5} TransitionComponent={transition}>
      <App />
    </SnackbarProvider>
  </ThemeProvider>,
  document.querySelector('#root'),
);
