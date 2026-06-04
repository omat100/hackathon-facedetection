module.exports = function (api) {
  api.cache(true);
  return {
    presets: ['babel-preset-expo'],
    plugins: [
      // This plugin translates your TS frame processor functions into raw high-speed native threads
      // ['react-native-worklets-core/plugin'],
    ],
  };
};