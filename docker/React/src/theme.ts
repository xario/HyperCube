import { createTheme } from '@mui/material/styles';
import { red } from '@mui/material/colors';

// A custom theme for this app
const theme = createTheme({
  breakpoints: {
    values: {
      xs: 0,
      sm: 650,
      md: 900,
      lg: 1200,
      xl: 1536,
    },
  },
  palette: {
    mode: 'dark',
    primary: {
      main: '#556cd6',
    },
    secondary: {
      main: '#19857b',
    },
    error: {
      main: red.A400,
    },
  },
  components: {
    MuiPaper: {
      styleOverrides: {
        root: { 
          '& .MuiDialog-paper': {
            overflowY: 'visible' 
          }
        }
      }
    }
  }
});

export default theme;
