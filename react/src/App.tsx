import * as React from 'react';
import fetch from './fetchWithTimeout'
import Container from '@mui/material/Container';
import Tabs from '@mui/material/Tabs';
import Tab from '@mui/material/Tab';
import Box from '@mui/material/Box';
import Link from '@mui/material/Link';
import TextField from '@mui/material/TextField';
import Autocomplete from '@mui/material/Autocomplete';
import Accordion from '@mui/material/Accordion';
import AccordionSummary from '@mui/material/AccordionSummary';
import AccordionDetails from '@mui/material/AccordionDetails';
import Typography from '@mui/material/Typography';
import Button from '@mui/material/Button';
import List from '@mui/material/List';
import ListItemText from '@mui/material/ListItemText';
import DialogContentText from '@mui/material/DialogContentText';

import DeleteIcon from '@mui/icons-material/Delete';
import CheckIcon from '@mui/icons-material/Check';
import EditIcon from '@mui/icons-material/Edit';

import MenuItem from '@mui/material/MenuItem';
import InputLabel from '@mui/material/InputLabel';
import FormControl from '@mui/material/FormControl';
import Select, { SelectChangeEvent } from '@mui/material/Select';
import Card from '@mui/material/Card';
import CardContent from '@mui/material/CardContent';
import Stack from '@mui/material/Stack';
import { ListItem } from '@mui/material';
import IconButton from '@mui/material/IconButton';

import Paper from '@mui/material/Paper';
import InputBase from '@mui/material/InputBase';

import Dialog from '@mui/material/Dialog';
import DialogActions from '@mui/material/DialogActions';
import DialogContent from '@mui/material/DialogContent';
import DialogTitle from '@mui/material/DialogTitle';
import { VariantType, useSnackbar } from 'notistack';

interface Data {
  apiKeys: ApiKeys;
  sides: Map<string, Side>;
}
interface ApiKeys {
  clockify?: (string)[];
  jira: string;
}
interface Side {
  name: string;
  clockify: ClockifyTimerConfig[];
}

interface ClockifyTimerConfig {
  apiKey: string;
  workspaceId: string;
  projectId: string;
  taskId: string;
  taskDesc: string;
}

interface TabPanelProps {
  children?: React.ReactNode;
  index: any;
  value: any;
}

export interface Workspace {
  hourlyRate:        HourlyRate;
  id:                string;
  imageUrl:          string;
  memberships:       Membership[];
  name:              string;
  workspaceSettings: WorkspaceSettings;
}

export interface HourlyRate {
  amount:   string;
  currency: string;
}

export interface Membership {
  hourlyRate:       HourlyRate;
  membershipStatus: string;
  membershipType:   string;
  targetId:         string;
  userId:           string;
}

export interface WorkspaceSettings {
  adminOnlyPages:                     any[];
  automaticLock:                      AutomaticLock;
  canSeeTimeSheet:                    string;
  canSeeTracker:                      string;
  defaultBillableProjects:            string;
  forceDescription:                   string;
  forceProjects:                      string;
  forceTags:                          string;
  forceTasks:                         string;
  lockTimeEntries:                    Date;
  onlyAdminsCreateProject:            string;
  onlyAdminsCreateTag:                string;
  onlyAdminsCreateTask:               string;
  onlyAdminsSeeAllTimeEntries:        string;
  onlyAdminsSeeBillableRates:         string;
  onlyAdminsSeeDashboard:             string;
  onlyAdminsSeePublicProjectsEntries: string;
  projectFavorites:                   string;
  projectGroupingLabel:               string;
  projectPickerSpecialFilter:         string;
  round:                              Round;
  timeRoundingInReports:              string;
  trackTimeDownToSecond:              string;
  isProjectPublicByDefault:           string;
  featureSubscriptionType:            string;
}

export interface AutomaticLock {
  changeDay:       string;
  dayOfMonth:      string;
  firstDay:        string;
  olderThanPeriod: string;
  olderThanValue:  string;
  type:            string;
}

export interface Round {
  minutes: string;
  round:   string;
}

export interface Project {
  id:             string;
  name:           string;
  hourlyRate:     null;
  clientId:       string;
  workspaceId:    string;
  billable:       boolean;
  memberships:    Membership[];
  color:          string;
  estimate:       Estimate;
  archived:       boolean;
  duration:       string;
  clientName:     string;
  note:           string;
  template:       boolean;
  public:         boolean;
  costRate:       null;
  budgetEstimate: null;
  timeEstimate:   TimeEstimate;
}

export interface Estimate {
  estimate: string;
  type:     string;
}

export interface TimeEstimate {
  estimate:    string;
  type:        string;
  resetOption: null;
  active:      boolean;
}

export interface Task {
  assigneeIds: string[];
  estimate:    string;
  id:          string;
  name:        string;
  projectId:   string;
  billable:    string;
  hourlyRate:  Rate;
  costRate:    Rate;
  status:      string;
}

export interface Rate {
  amount:   string;
  currency: string;
}

export interface ProjectOption {
  label: string;
  id: string;
}

class SideMaker {
  static create(): Side {
    return { name: "new side", clockify: [] };
  }
}

class ClockifyTimerConfigMaker {
  static create(apiKey: string,  workspaceId: string, projectId: string, taskId: string, taskDesc: string): ClockifyTimerConfig {
    return { 
      apiKey: apiKey, 
      workspaceId: workspaceId,
      projectId: projectId,
      taskId: taskId !== "-" ? taskId : "",
      taskDesc: taskDesc 
    };
  }
}

function TabPanel(props: TabPanelProps) {
  const { children, value, index, ...other } = props;

  return (
    <div
      role="tabpanel"
      hidden={value !== index}
      id={`simple-tabpanel-${index}`}
      aria-labelledby={`simple-tab-${index}`}
      {...other}
    >
      {value === index && (
        <Box p={3}>
          <Typography component="span">{children}</Typography>
        </Box>
      )}
    </div>
  );
}

export default function App() {
  const hostname = location.hostname;
  //const hostname = "192.168.200.199";

  const [data, setData]= React.useState<Data | null>(null);
  const [socket, setSocket]= React.useState<WebSocket | null>(null);

  const [, updateState] = React.useState(0);
  const forceUpdate = React.useCallback(() => updateState((tick) => tick + 1), []);
    
  async function updateData(data: Data | null) {
    let d = {
      apiKeys: data?.apiKeys,
      sides: Object()
    };
    
    data?.sides?.forEach((side: Side, key: string) => {
      d.sides[key] = side;
    });

    const json = JSON.stringify(d);
    await fetch('http://' + hostname + '/data', {
      method: 'POST',
      body: json
    });

    setData(data);
  }

  let connectInterval: NodeJS.Timer = null;
  let socketInit = false;
  let socketConnected = false;
  let dataLoaded = false;

  React.useEffect(() => {
    async function getData() {
      if (data || dataLoaded) {
        return;
      }
      
      try {
        const res = await fetch('http://' + hostname + '/data');
        const d: Data = await res.json();
        const m = new Map<string, Side>();
        d.sides = new Map(Object.entries(d.sides));
        setData(d);
        dataLoaded = true;
      } catch(e) {
        console.log(e);
      }      
    }

    async function getWSToken() {
      try {
        let res = await fetch('http://' + hostname + '/token', {
          method: "GET",
        }).then(res => res.json());
    
        return res["token"];
      } catch(e) {
        console.log(e);
      }      
    }
    
    async function openWebsocketConnection() {
      if (socket != null || socketInit || socketConnected || !data) {
        return;
      }

      let token = await getWSToken();
      if (!token) {
        return;
      }

      socketInit = true;
      
      let uri = 'ws://' + hostname + ':81/';
      let newUri = uri.substring(0, uri.indexOf("//") + 2) + token + '@' + uri.substring(uri.indexOf("//") + 2);
      let s = new WebSocket(newUri);
      let pingTimeout = null;
      s.onmessage = function (msg) {
        if (msg.data === "pong") {
          clearTimeout(pingTimeout);
          return;
        }
    
        let sideId: string = msg.data;
        if (currentSide !== sideId) {        
          handleClockifyTimerDialogClose();
        }      
        setCurrentSide(sideId);
    
        if (sideId !== "")  {
          if (!data.sides.has(sideId)) {
            data.sides.set(sideId, SideMaker.create());
            updateData(data);
            forceUpdate();
          }
        }
      };;

      s.onerror = function() {
        socketConnected = false;
        socketInit = false;
        setSocket(null);
      };

      s.onclose = function() {
        socketConnected = false;
        socketInit = false;
        setSocket(null);
      };

      s.onopen = function() {
        setInterval(() => {
          s.send("ping");
          pingTimeout = setTimeout(() => {
            s.close();
          }, 4000);
        }, 5000);        
        socketInit = false;
        socketConnected = true;
      };

      setSocket(s);
    }

    if (!connectInterval) {
      getData();    
      connectInterval = setInterval(async function() {
        await getData();
        await openWebsocketConnection();    
      }, 2000);
    }
  }, [data]);

  const [value, setValue] = React.useState(0);

  const handleChange = (event: React.SyntheticEvent, newValue: number) => {
    setValue(newValue);
  };

  const [checked, setChecked] = React.useState([true, false]);
  
  const [badApiKey, setBadApiKey] = React.useState(false);
  const [selectedApiKey, setSelectedApiKey] = React.useState('-');
  const handleApiKeySelectChange = (event: SelectChangeEvent) => {
    let key = event.target.value;
    setSelectedApiKey(key);
    resetWorkspaces();
    if (key !== "-") {
      fetchWorkspaces(key);
    }
  };

  const resetWorkspaces = () => {
    setSelectedWorkspace("-");
    setWorkspaces(Array<Workspace>());
    resetProjects();
  };

  const [workspaces, setWorkspaces] = React.useState(Array<Workspace>());
  const [badWorkspace, setBadWorkspace] = React.useState(false);
  const [selectedWorkspace, setSelectedWorkspace] = React.useState('-');
  const handleWorkspaceSelectChange =  (event: SelectChangeEvent) => {
    let workspaceId = event.target.value;
    setSelectedWorkspace(workspaceId);
    resetProjects();
    if (workspaceId !== "-") {
      fetchProjects(selectedApiKey, workspaceId);
    }
  };

  const resetProjects = () => {
    setSelectedProject(defaultProjectOption);
    setProjects(defaultProjectOptions);
    resetTasks();
  };

  const defaultProjectOption: ProjectOption = {label: '-', id: '-'};
  const defaultProjectOptions: Array<ProjectOption> = [defaultProjectOption];
  const [projects, setProjects] = React.useState(defaultProjectOptions);
  const [badProject, setBadProject] = React.useState(false);
  const [selectedProject, setSelectedProject] = React.useState(defaultProjectOption);
  const handleProjectSelectChange = (event: React.SyntheticEvent, target: any) => {
    if (target === null) {
      resetTasks();
      return;
    }

    let option: ProjectOption = target;
    setSelectedProject(option);
    resetTasks();
    if (option.id !== "-") {
      fetchTasks(selectedApiKey, selectedWorkspace, option.id); 
    }
  };

  const resetTasks = () => {
    setSelectedTask("-");
    setTasks(Array<Task>());
  };

  const [tasks, setTasks] = React.useState(Array<Task>());
  const [selectedTask, setSelectedTask] = React.useState('-');
  const handleTaskSelectChange = (event: SelectChangeEvent) => {
    let taskId = event.target.value;
    setSelectedTask(taskId);
  };

  const [badDescription, setBadDescription] = React.useState(false);
  const [taskDescription, setTaskDescription] = React.useState('');

  const validateClockifyTimerForm = () => {
    let valid = true;
    let b: boolean;
    
    if (b = (selectedApiKey === '-')) {
      showError("API key not selected!");
      valid = false;
    }

    if (!data) {
      showError("Data not loaded!");
      return false;
    }

    const side = data.sides.get(currentSide);
      if (side) {
        side.clockify.forEach((timerConfig: ClockifyTimerConfig, id: number) => {
          if (timerConfig.apiKey === selectedApiKey && id !== clockifyTimerEditingId) {
            valid = false
            b = true;
            showError("API key already used in other config on this side!");
          }
        });
      }
    setBadApiKey(b);

    if (b = (selectedWorkspace === '-')) {
      showError("Workspace not selected!");
      valid = false;
    }
    setBadWorkspace(b);

    if (b = (selectedProject.label === "-")) {
      showError("Project not selected!");
      valid = false;
    }
    setBadProject(b);

    if (b = (taskDescription === '')) {
      showError("No description entered!");
      valid = false;
    }
    setBadDescription(b);

    return valid;
  };

  const [addingKey, setAddingKey] = React.useState(false);
  const onAddKey = (event: React.MouseEvent) => {
    setAddingKey(true);
  };

  const [keyError, setKeyError] = React.useState(false)

  const addKey = async (event: React.MouseEvent) => {
    if (data && data.apiKeys.clockify && data.apiKeys.clockify.indexOf(newKey) > -1) {
      setKeyError(true);
      return;
    }

    if (!await isKeyValid(newKey)) {
      setKeyError(true);
      showError("Bad API key!")
      return;
    }

    setKeyError(false);
    setAddingKey(false);
    if (data != null) {
      data.apiKeys.clockify?.push(newKey);
    }

    updateData(data);
    setNewKey("");
    showSuccess("API key added!")
  };

  const [keyIdToBeDeleted, setKeyIdToBeDeleted] = React.useState(-1)
  const showApiKeyDeleteDialog = (keyId: number) => {
    setKeyIdToBeDeleted(keyId);
    setDeleteApiKeyDialogOpen(true);
  };

  const deleteKey = async () => {
    if (data == null || data.apiKeys.clockify == null) {
      return;
    }
    

    const key = data.apiKeys.clockify[keyIdToBeDeleted];
    data.apiKeys.clockify?.splice(keyIdToBeDeleted, 1);
    data.sides.forEach((side) => {
      for (let i = 0; i < side.clockify.length; i++) {
        let c = side.clockify[i];
        if (c.apiKey === key) {
          side.clockify.splice(i, 1);
        }
      }
    });

    updateData(data);
    forceUpdate();
    showSuccess("API key deleted!")
    setDeleteApiKeyDialogOpen(false);
    setKeyIdToBeDeleted(-1);
  };

  const [newKey, setNewKey] = React.useState("");

  const isKeyValid = async (key:string) => {
    let res = true;
    await fetch('https://api.clockify.me/api/v1/user', {
      method: 'GET',
      headers: {
        'X-Api-Key': key,
        'Content-Type': 'application/json'
      }
    }).then((response) => {
      res = response.status < 400;
    }).catch((error) => {
      res = false;
    });

    return res;
  }

  const onResetWifi = (event: React.MouseEvent) => {
    setResetWifiDialogOpen(true);
  };

  const resetWifi = async (event: React.MouseEvent) => {
    await fetch('http://' + hostname + '/wifi', {
      method: 'DELETE'
    });
    showSuccess("WiFi configation reset");
    window.location.reload();
  };

  const onFactoryReset = (event: React.MouseEvent) => {
    setFactoryResetDialogOpen(true);
  };

  const factoryReset = async (event: React.MouseEvent) => {
    await fetch('http://' + hostname + '/reset', {
      method: 'POST'
    });
    showSuccess("Factory reset completed");
    window.location.reload();
  };

  const fetchWorkspaces = async (key: string) => {
    let res = await fetch('https://api.clockify.me/api/v1/workspaces', {
      method: 'GET',
      headers: {
        'X-Api-Key': key,
        'Content-Type': 'application/json'
      }
    }).then(r => r.json());

    setWorkspaces(res);
  }

  const fetchProjects = async (apiKey: string, workspaceId: string) => {
    let res = await fetch('https://api.clockify.me/api/v1/workspaces/' + workspaceId + '/projects?page-size=1000', {
      method: 'GET',
      headers: {
        'X-Api-Key': apiKey,
        'Content-Type': 'application/json'
      }
    }).then(r => r.json());

    let projects: Array<Project> = res;
    let options: Array<ProjectOption> = projects.map(el => ({id: el.id, label: el.name}))
    options.unshift(defaultProjectOption);
    setProjects(options);

    return options;
  }

  const fetchTasks = async (apiKey: string, workspaceId: string, projectId: string) => {
    let res = await fetch('https://api.clockify.me/api/v1/workspaces/' + workspaceId + '/projects/' + projectId + '/tasks?page-size=1000', {
      method: 'GET',
      headers: {
        'X-Api-Key': apiKey,
        'Content-Type': 'application/json'
      }
    }).then(r => r.json());

    setTasks(res);

    return res;
  }

  const [currentSide, setCurrentSide] = React.useState("");

  const [clockifyTimerEditingId, setClockifyTimerEditingId] = React.useState(-1);

  const [clockifyTimerEditDialogOpen, setClockifyTimerEditDialogOpen] = React.useState(false);

  const resetClockifyTimerForm = () => {
    setBadApiKey(false);
    setBadWorkspace(false);
    setBadProject(false);
    setBadDescription(false);
    setSelectedApiKey("-");
    setSelectedWorkspace("-");
    setSelectedProject(defaultProjectOption);
    setSelectedTask("-");
    setTaskDescription("");
  };

  const handleClockifyTimerEdit = async (configId: number) => {
    if (!data) {
      return false;
    }

    const side = data.sides.get(currentSide);
    if (!side) {
      return false;
    }

    const config: ClockifyTimerConfig = side.clockify[configId];
    console.log(config);
    setClockifyTimerEditingId(configId);

    resetClockifyTimerForm();
    setSelectedApiKey(config.apiKey);
    await fetchWorkspaces(config.apiKey);

    setSelectedWorkspace(config.workspaceId);
    let projects = await fetchProjects(config.apiKey, config.workspaceId);
    const filteredProjects = projects.filter((value, index) => {
      return value.id === config.projectId;
    });
    if (filteredProjects.length > 0) {
      setSelectedProject(filteredProjects[0]);

      await fetchTasks(config.apiKey, config.workspaceId, config.projectId);
      setSelectedTask(config.taskId == '' ? '-' : config.taskId);  
    }    

    setTaskDescription(config.taskDesc);

    setClockifyTimerEditDialogOpen(true);
  };

  const handleClockifyTimerDelete = (configId: number) => {
    if (!data) {
      return false;
    }

    const side = data.sides.get(currentSide);
    if (!side) {
      return false;
    }
    
    side.clockify.splice(configId, 1);
    data.sides.set(currentSide, side);
    forceUpdate();
    updateData(data);
    showSuccess("Config deleted!");
  };

  const handleClockifyTimerDialogClose = () => {
    setClockifyTimerEditingId(-1);
    setClockifyTimerEditDialogOpen(false);
  };

  const storeClockifyTimerConfig = () => {
    if (!data) {
      return false;
    }

    const side = data.sides.get(currentSide);    
    if (!side) {
      return false;
    }

    const config = ClockifyTimerConfigMaker.create(
      selectedApiKey,
      selectedWorkspace,
      selectedProject.id,
      selectedTask,
      taskDescription
    );

    if (clockifyTimerEditingId >= 0) {
      side.clockify[clockifyTimerEditingId] = config;
    } else {
      side.clockify.push(config);
    }

    updateData(data);
    setClockifyTimerEditingId(-1);
  };

  const handleClockifyTimerDialogSave = (event: React.MouseEvent) => {
    event.preventDefault();
    event.stopPropagation();
    if (!validateClockifyTimerForm()) {      
      return false;
    }

    setClockifyTimerEditDialogOpen(false);
    storeClockifyTimerConfig();
  };

  const handleClockifyTimerAdd = () => {
    setClockifyTimerEditingId(-1);
    resetClockifyTimerForm();
    setClockifyTimerEditDialogOpen(true);
  };

  const handleAccordionChange = (event: React.SyntheticEvent, expanded: boolean) => {    
    event.preventDefault();
    event.stopPropagation();

    if (expanded && data) {
      const side = data.sides.get(currentSide);
      if (!side) {
        showError("Side not configured!");
        return;
      }

      setNewSideName(side.name);
    }
  }

  const [newSideName, setNewSideName] = React.useState("");

  const handleSideNameClick = (event: React.MouseEvent) => {
    event.preventDefault();
    event.stopPropagation();

    if (!data) {
      showError("Data not loaded!");
      return;
    }
    
    const side = data.sides.get(currentSide);
    if (!side) {
      showError("Side not configured!");
      return;
    }

    side.name = newSideName;
    forceUpdate();
    showSuccess("Side name updated!");
  }

  const { enqueueSnackbar } = useSnackbar();

  const showAlert = (message: string, variant: VariantType) => {
    enqueueSnackbar(message, { variant });
  };
  
  const showSuccess = (message: string) => {
    showAlert(message, 'success');
  };

  const showError = (message: string) => {
    showAlert(message, 'error');
  };

  const showWarning = (message: string) => {
    showAlert(message, 'warning');
  };

  const showInfo = (message: string) => {
    showAlert(message, 'info');
  };

  const [deleteApiKeyDialogOpen, setDeleteApiKeyDialogOpen] = React.useState(false);

  const handleDeleteApiKeyDialogClose = () => {
    setDeleteApiKeyDialogOpen(false);
  };

  const [resetWifiDialogOpen, setResetWifiDialogOpen] = React.useState(false);

  const handleResetWifiDialogClose = () => {
    setResetWifiDialogOpen(false);
  };

  const [factoryResetDialogOpen, setFactoryResetDialogOpen] = React.useState(false);

  const handleFactoryResetDialogClose = () => {
    setFactoryResetDialogOpen(false);
  };

  const downloadFile = async () => {
    const fileName = "data";
    let d = {
      apiKeys: data?.apiKeys,
      sides: Object()
    };
    
    data?.sides.forEach((side: Side, key: string) => {
      d.sides[key] = side;
    });

    const json = JSON.stringify(d);
    const blob = new Blob([json],{type:'application/json'});
    const href = await URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = href;
    link.download = fileName + ".json";
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  const uploadFile = (e: React.ChangeEvent<HTMLInputElement>) => {
    const fileReader = new FileReader();
    fileReader.readAsText(e.target.files[0], "UTF-8");
    fileReader.onload = e => {
      let d: Data = JSON.parse(e.target.result.toString())
      d.sides = new Map(Object.entries(d.sides));
      updateData(d);
      forceUpdate();
    };
  };

  return (
    <Container maxWidth="sm">
      <Tabs value={value} onChange={handleChange} centered>
        <Tab label="API keys" />
        <Tab label="General config" />
        <Tab label="Cube config" />
      </Tabs>
      <TabPanel value={value} index={0}>
      <Card raised>
        <CardContent>
          <Stack spacing={2}>
            <Typography variant="h6">Clockify API keys</Typography>
            <Typography>Go to <Link href="https://clockify.me/user/settings#:~:text=API" target="_blank" rel="noreferrer">https://clockify.me/user/settings</Link> to generate a new API key (bottom of page).</Typography>
            <Typography>Multiple API keys can be added to enable time tracking in parallel across multiple accounts and projects.</Typography>
            <List>
              {data ? data.apiKeys.clockify?.map((apiKey, i) => (
              <ListItem
                  key={i}
                  divider
                  secondaryAction={                      
                    <IconButton key={i} edge="end" aria-label="delete" onClick={() => showApiKeyDeleteDialog(i)}>
                      <DeleteIcon key={i} />
                    </IconButton>
                  }
                >
                <Typography key={i}>{apiKey}</Typography>
              </ListItem>         
              )) : 
              <ListItem key="loading" divider>
                <Typography>Loading</Typography>
              </ListItem>} 
              {addingKey ? <ListItem
                  key="adding"
                  divider
                  secondaryAction={
                    <IconButton edge="end" aria-label="add key" onClick={addKey}>
                      <CheckIcon />
                    </IconButton>
                  }
                >
                  <TextField
                    size="small"
                    fullWidth
                    error={keyError}
                    label= "API key"
                    onChange={event => setNewKey(event.target.value)}
                  />
              </ListItem>
              : ""}
            </List>
            <Button sx={{display: addingKey ? 'none': 'inherit'}} variant="outlined" size="large" onClick={onAddKey}>Add new API key</Button>
          </Stack>
        </CardContent>
      </Card>
      </TabPanel>
      <TabPanel value={value} index={1}>
        <Stack spacing={2}>
          <Card raised>
            <CardContent>
              <Stack spacing={2}>
                <Typography variant="h6">WiFi config</Typography>
                <Button variant="outlined" size="large" onClick={onResetWifi}>Reset WiFi</Button>
                <Typography>This will erase any saved WiFi configurations and reset the current connection. When the base tries to reconnect, it will launch an access point (AP) to allow addtion of new WiFi configurations.</Typography>
              </Stack>
            </CardContent>
          </Card>
          <Card raised>
            <CardContent>
              <Stack spacing={2}>
                <Typography variant="h6">Factory reset</Typography>
                <Button variant="outlined" size="large" onClick={onFactoryReset}>Factory reset</Button>
                <Typography>This will erase all saved configuration, including WiFi credentials.</Typography>
              </Stack>
            </CardContent>
          </Card>
          <Card raised>
            <CardContent>
              <Stack spacing={2}>
                <Typography variant="h6">Config</Typography>
                <Button variant="outlined" size="large" onClick={downloadFile}>Download</Button>
                <Typography>Download a copy of the current cube configuration.</Typography>
                <input
                  accept="application/json"
                  style={{ display: 'none' }}
                  id="raised-button-file"
                  type="file"
                  onChange={uploadFile}
                />
                <label htmlFor="raised-button-file">
                  <Button variant="outlined" size="large" style={{ width: '100%' }} component="span">
                    Upload
                  </Button>
                </label>
                <Typography>Upload a saved copy to replace the current cube configuration.</Typography>
              </Stack>
            </CardContent>
          </Card>
        </Stack>
      </TabPanel>
      <TabPanel value={value} index={2}>
        <Stack spacing={2}>
          <Card raised>
            <CardContent>
              <Stack spacing={2}>
                <Typography variant="h6">How to configure</Typography>
                <Typography>This section is for configuring actions occouring when a cube is placed on the base. Each side of the cube can be configured. In order to configure a side, place it on the base facing up. When the cube is placed, the releated configuration will expand in the accordion below. If the side has not been previously configured a new section will be created.</Typography>
              </Stack>
            </CardContent>
          </Card>          
          <Card raised>
            <CardContent>
              <Typography variant="h6" marginBottom={2}>Cube sides</Typography>
              {data ? data.sides.size ? [...data.sides].map(([key, side]) => (
              <Accordion key={"a" + key} disabled={true} expanded={currentSide === key} onChange={handleAccordionChange}>
              <AccordionSummary key={"as" + key}>
                <Typography>{side.name}</Typography>
              </AccordionSummary>
              <AccordionDetails key={"ad" + key}>
                <Stack spacing={1}>
                  <Card raised>
                    <CardContent>
                      <Stack spacing={2}>
                        <Typography variant="h6">Side name</Typography>
                        <Paper
                          component="form"
                          sx={{ p: '2px 4px', display: 'flex', alignItems: 'center', height: '40px' }}
                        >
                          <InputBase
                            sx={{ ml: 1, flex: 1 }}
                            placeholder="Enter name of side"
                            onChange={(event) => setNewSideName(event.target.value)}
                            inputProps={{ 'aria-label': 'Name', 'defaultValue': side.name, 'label': 'Name' }}
                          />
                          <IconButton type="submit" sx={{ p: '10px', display: side.name === newSideName || newSideName === "" ? "none" : "inherit" }} aria-label="save" onClick={handleSideNameClick}>
                            <CheckIcon/>
                          </IconButton>
                        </Paper>
                        <Typography>This helps identifying if the correct side has been located. The name can reflect the text written on the side of the cube.</Typography>
                      </Stack>
                    </CardContent>
                  </Card>
                  <Card raised>
                    <CardContent>
                      <Stack spacing={2}>
                        <Typography variant="h6">Clockify timer configs</Typography>
                        <List>
                          {side.clockify.length ? side.clockify.map((timer, i) => (
                          <ListItem key={i} divider>
                            <ListItemText primary={timer.taskDesc} />
                            <IconButton edge="end" aria-label="edit" onClick={() => handleClockifyTimerEdit(i)}>
                              <EditIcon />
                            </IconButton>
                            <IconButton edge="end" aria-label="delete"  onClick={() => handleClockifyTimerDelete(i)}>
                              <DeleteIcon />
                            </IconButton>
                          </ListItem>)) : 
                          <ListItem key={"noTimers"}>
                            <Typography>No timer configurations added</Typography>
                          </ListItem>}
                        </List>
                        <Button variant="outlined" size="large" onClick={handleClockifyTimerAdd}>Add new timer config</Button>
                      </Stack>
                    </CardContent>
                  </Card>
                </Stack>
              </AccordionDetails>
            </Accordion>)) : 
              <Typography>No sides added</Typography> : 
              <Typography>Loading</Typography>}
            </CardContent>
          </Card>
        </Stack>
      </TabPanel>
      <Dialog 
        open={clockifyTimerEditDialogOpen} 
        onClose={handleClockifyTimerDialogClose}
        PaperProps={{ sx: { overflowY: 'visible' } }}
        fullWidth
      >
        <DialogTitle>{(clockifyTimerEditingId > -1 ? "Edit" : "Create") + " timer config"}</DialogTitle>
        <DialogContent sx={{overflowY: 'visible' }}>
          <Stack spacing={1}>  
            <FormControl fullWidth margin="normal">
              <InputLabel>API key</InputLabel>
              <Select
                size="small"
                value={selectedApiKey}
                defaultValue={selectedApiKey}
                error={badApiKey}
                label="API key"
                onChange={handleApiKeySelectChange}
              >
                <MenuItem value="-">-</MenuItem>
                {data ? data.apiKeys.clockify?.map(el => (<MenuItem value={el}>{el}</MenuItem>)) : <p>Loading</p>}
              </Select>
            </FormControl>
            
            <FormControl fullWidth margin="normal">
              <InputLabel>Workspace</InputLabel>
              <Select
                size="small"
                value={selectedWorkspace}
                defaultValue={selectedWorkspace}
                error={badWorkspace}
                label="Workspace"
                onChange={handleWorkspaceSelectChange}
              >
                <MenuItem value="-">-</MenuItem>
                {workspaces.length ? workspaces.map(el => (<MenuItem value={el.id}>{el.name}</MenuItem>)) : <p>Loading</p>}
              </Select>
            </FormControl>
            <FormControl fullWidth margin="normal">
              <Autocomplete
                size="small"
                disablePortal
                id="projectSelector"
                disableClearable={true}
                value={selectedProject}
                defaultValue={selectedProject}
                isOptionEqualToValue={(option, value) => option.id === value.id}         
                options={projects}
                renderInput={(params) => <TextField {...params} error={badProject} label="Project"/>}
                limitTags={5}
                onChange={handleProjectSelectChange}                
              />
            </FormControl>
            <FormControl fullWidth margin="normal">
              <InputLabel>Task</InputLabel>
              <Select
                size="small"
                value={selectedTask}
                defaultValue={selectedTask}
                label="Task"
                onChange={handleTaskSelectChange}
              >
                <MenuItem value="-">-</MenuItem>
                {tasks.length ? tasks.map(el => (<MenuItem value={el.id}>{el.name}</MenuItem>)) : ""}
              </Select>
            </FormControl>
            <FormControl fullWidth margin="normal">
              <TextField 
                size="small" 
                defaultValue={taskDescription} 
                onChange={(event) => {setTaskDescription(event.target.value)} }
                error={badDescription}
                label="Description"
              />
            </FormControl>
          </Stack>
        </DialogContent>
        <DialogActions>
          <Button onClick={handleClockifyTimerDialogClose}>Cancel</Button>
          <Button onClick={handleClockifyTimerDialogSave}>Save</Button>
        </DialogActions>
      </Dialog>
      <Dialog
        fullScreen={false}
        open={deleteApiKeyDialogOpen}
        onClose={handleDeleteApiKeyDialogClose}
        aria-labelledby="responsive-dialog-title"
      >
        <DialogTitle id="responsive-dialog-title">
          {"Delete API key?"}
        </DialogTitle>
        <DialogContent>
          <DialogContentText>
            All side configurations using this API key will be deleted along with the key, are you sure you want to continue?
          </DialogContentText>
        </DialogContent>
        <DialogActions>
          <Button autoFocus onClick={handleDeleteApiKeyDialogClose}>
            No
          </Button>
          <Button onClick={deleteKey} autoFocus>
            Yes
          </Button>
        </DialogActions>
      </Dialog>
      <Dialog
        fullScreen={false}
        open={resetWifiDialogOpen}
        onClose={handleResetWifiDialogClose}
        aria-labelledby="responsive-dialog-title"
      >
        <DialogTitle id="responsive-dialog-title">
          {"Reset WiFi configuration?"}
        </DialogTitle>
        <DialogContent>
          <DialogContentText>
            All WiFi configration will be deleted and the device will reboot, are you sure you want to continue?
          </DialogContentText>
        </DialogContent>
        <DialogActions>
          <Button autoFocus onClick={handleResetWifiDialogClose}>
            No
          </Button>
          <Button onClick={resetWifi} autoFocus>
            Yes
          </Button>
        </DialogActions>
      </Dialog>
      <Dialog
        fullScreen={false}
        open={factoryResetDialogOpen}
        onClose={handleFactoryResetDialogClose}
        aria-labelledby="responsive-dialog-title"
      >
        <DialogTitle id="responsive-dialog-title">
          {"Factory reset?"}
        </DialogTitle>
        <DialogContent>
          <DialogContentText>
            All stored configuration (including WiFi) will be deleted and the device will reboot, are you sure you want to continue?
          </DialogContentText>
        </DialogContent>
        <DialogActions>
          <Button autoFocus onClick={handleFactoryResetDialogClose}>
            No
          </Button>
          <Button onClick={factoryReset} autoFocus>
            Yes
          </Button>
        </DialogActions>
      </Dialog>
    </Container>
  );
}
