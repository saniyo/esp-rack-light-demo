import { FC, useCallback, useContext, useEffect } from 'react';
import { Navigate, Routes, Route, useNavigate, useLocation } from 'react-router-dom';
import { AxiosError } from 'axios';

import { FeaturesContext } from './contexts/features';
import PresenceProvider from './contexts/presence/PresenceProvider';
import * as AuthenticationApi from './api/authentication';
import { PROJECT_PATH } from './api/env';
import { AXIOS } from './api/endpoints';
import { Layout, RequireAdmin } from './components';

import ProjectRouting from './project/ProjectRouting';

import System from './framework/system/System';
import Security from './framework/security/Security';
import FileSystem from './framework/filesystem/FileSystem';
import DynamicFeature from './components/dynamic-component/DynamicFeature';

const AuthenticatedRouting: FC = () => {
  const { features } = useContext(FeaturesContext);
  const location = useLocation();
  const navigate = useNavigate();

  const handleApiResponseError = useCallback((error: AxiosError) => {
    if (error.response && error.response.status === 401) {
      AuthenticationApi.storeLoginRedirect(location);
      navigate("/unauthorized");
    }
    return Promise.reject(error);
  }, [location, navigate]);

  useEffect(() => {
    const axiosHandlerId = AXIOS.interceptors.response.use((response) => response, handleApiResponseError);
    return () => AXIOS.interceptors.response.eject(axiosHandlerId);
  }, [handleApiResponseError]);

  return (
    <PresenceProvider>
    <Layout>
      <Routes>
        {features.project && (
          <Route path={`/${PROJECT_PATH}/*`} element={<ProjectRouting />} />
        )}
        <Route path="/wifi/*" element={<DynamicFeature featureId="wifi" />} />
        <Route path="/ap/*" element={<DynamicFeature featureId="ap" />} />
        {features.ntp && (
          <Route path="/ntp/*" element={<DynamicFeature featureId="ntp" />} />
        )}
        {features.mqtt && (
          <Route path="/mqtt/*" element={<DynamicFeature featureId="mqtt" />} />
        )}
        {features.telegram && (
          <Route path="/telegram/*" element={<DynamicFeature featureId="telegram" />} />
        )}
        {features.security && (
          <Route
            path="/security/*"
            element={
              <RequireAdmin>
                <Security />
              </RequireAdmin>
            }
          />
        )}
        <Route
          path="/filesystem/*"
          element={
            <RequireAdmin>
              <FileSystem />
            </RequireAdmin>
          }
        />
        <Route path="/system/*" element={<System />} />
        <Route path="/*" element={<Navigate to={AuthenticationApi.getDefaultRoute(features)} />} />
      </Routes>
    </Layout>
    </PresenceProvider>
  );
};

export default AuthenticatedRouting;
