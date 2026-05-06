import { FC, useEffect, useMemo, useState } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';
import { Tab } from '@mui/material';
import { AxiosPromise } from 'axios';

import { RouterTabs, useRouterTab, useLayoutTitle } from '../components';
import DynamicStatus from '../components/dynamic-component/DynamicStatus';
import DynamicSettings from '../components/dynamic-component/DynamicSettings';
import { AXIOS, WEB_SOCKET_ROOT } from '../api/endpoints';
import { readState, updateState, LIGHT_SETTINGS_WEBSOCKET_URL } from './api';
import { useRest } from '../utils';
import { useWs } from '../utils/useWs';
import FormLoader from '../components/loading/FormLoader';
import { useManifest } from '../contexts/manifest';
import { TabSpec } from '../types';

// Fallback tab layout when the manifest is empty — mirrors the legacy
// hardcoded behavior so the page keeps working if /rest/uiManifest is down.
const FALLBACK_TABS: TabSpec[] = [
  { key: 'status', title: 'Status', restPath: '/rest/lightState', postable: false, live: true },
  { key: 'settings', title: 'Settings', restPath: '/rest/lightState', postable: true, live: false },
];

// AXIOS baseURL is '/rest/'. The manifest publishes absolute paths like
// '/rest/lightState'; strip the '/rest' prefix before handing to axios or
// we end up with '/rest//rest/lightState' which the server rewrites into
// the SPA index.html fallback (seen as HTML in REST responses).
const toAxiosPath = (absPath: string) =>
  absPath.startsWith('/rest/') ? absPath.slice(5) : absPath;

const LightControl: FC = () => {
  useLayoutTitle("Light Control");
  const { routerTab } = useRouterTab();
  const { findFeature } = useManifest();

  // Resolve URLs + tabs from the manifest when available, fall back to the
  // legacy hardcoded values. All interaction mode information (live/postable)
  // is driven by the C++ WebFeatureSpec via /rest/uiManifest.
  const resolved = useMemo(() => {
    const entry = findFeature('lightState');
    const restRead = entry?.rest?.read ?? '/lightState';
    const restUpdate = entry?.rest?.update ?? '/lightState';
    const wsUrl = entry?.ws
      ? WEB_SOCKET_ROOT + entry.ws.replace(/^\/ws\//, '')
      : LIGHT_SETTINGS_WEBSOCKET_URL;
    const tabs = entry?.tabs && entry.tabs.length > 0 ? entry.tabs : FALLBACK_TABS;
    return { restRead, restUpdate, wsUrl, tabs };
  }, [findFeature]);

  const read = useMemo(
    () => (): AxiosPromise<Record<string, any>> => AXIOS.get(toAxiosPath(resolved.restRead)),
    [resolved.restRead]
  );
  const update = useMemo(
    () => (data: Record<string, any>): AxiosPromise<Record<string, any>> =>
      AXIOS.post(toAxiosPath(resolved.restUpdate), data),
    [resolved.restUpdate]
  );

  void readState;
  void updateState;

  const { loadData, saveData, saving, setData, data, errorMessage } = useRest<any>({
    read,
    update,
  });

  // Отримуємо wsData через useWs
  const { connected, wsData, updateData: updateWsData, disconnect } = useWs<any>(resolved.wsUrl);

  // Зберігаємо origin_id у локальному стані
  const [originId, setOriginId] = useState<string>("");
  useEffect(() => {
    if (wsData && wsData.origin_id) {
      setOriginId(wsData.origin_id);
    }
  }, [wsData]);

  useEffect(() => {
    loadData();
  }, [loadData]);

  const handleUpdateData = (formName: string, newData: Partial<any>) => {
    setData((prevData: any) => ({
      ...prevData,
      [formName]: {
        ...prevData[formName],
        ...newData
      }
    }));
  };

  const handleSaveData = (updatedData: Partial<any>) => {
    update(updatedData)
      .then(() => {
        saveData();
      })
      .catch((err) => {
        console.error(`Error saving data:`, err);
      });
  };

  if (!data) {
    return <FormLoader errorMessage={errorMessage} message="Loading REST data..." />;
  }

  const renderTab = (tab: TabSpec) => {
    const formData = data?.[tab.key];
    // Guard against malformed responses (e.g. server returned HTML or the
    // expected key is missing). Without this, DynamicSettings/DynamicStatus
    // crash on `data.description`.
    if (!formData || typeof formData !== 'object' || !Array.isArray((formData as any).fields)) {
      return <FormLoader message={`Loading ${tab.title ?? tab.key}...`} />;
    }
    // Tab interaction mode is configured server-side in the C++ WebFeatureSpec:
    //   tab.live      → subscribes to WS push updates (DynamicStatus)
    //   tab.postable  → writes back via POST (DynamicSettings)
    //   neither       → read-only snapshot via DynamicSettings without save
    if (tab.live) {
      return (
        <DynamicStatus
          formName={tab.title ?? tab.key}
          data={formData}
          wsData={wsData}
          connected={connected}
          saving={saving}
          originId={originId}
          setData={(newData: any) => handleUpdateData(tab.key, newData)}
          disconnect={disconnect}
          updateWsData={updateWsData}
        />
      );
    }
    return (
      <DynamicSettings
        formName={tab.title ?? tab.key}
        data={formData}
        saveData={tab.postable ? (updated: any) => handleSaveData(updated) : () => {}}
        saving={saving}
        setData={(newData: any) => handleUpdateData(tab.key, newData)}
      />
    );
  };

  const firstTabKey = resolved.tabs[0]?.key ?? 'status';

  return (
    <>
      <RouterTabs value={routerTab}>
        {resolved.tabs.map((t) => (
          <Tab key={t.key} value={t.key} label={t.title ?? t.key} />
        ))}
      </RouterTabs>
      <Routes>
        {resolved.tabs.map((t) => (
          <Route key={t.key} path={t.key} element={renderTab(t)} />
        ))}
        <Route path="/*" element={<Navigate replace to={firstTabKey} />} />
      </Routes>
    </>
  );
};

export default LightControl;
