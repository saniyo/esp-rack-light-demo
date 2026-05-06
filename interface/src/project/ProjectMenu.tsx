// interface/src/project/ProjectMenu.tsx
import { FC } from 'react';
import { List } from '@mui/material';
import SettingsRemoteIcon from '@mui/icons-material/SettingsRemote';
import { PROJECT_PATH } from '../api/env';
import LayoutMenuItem from '../components/layout/LayoutMenuItem';

const ProjectMenu: FC = () => (
  <List>
    <LayoutMenuItem icon={SettingsRemoteIcon} label="Light Control" to={`/${PROJECT_PATH}/light-control`} />
  </List>
);

export default ProjectMenu;
