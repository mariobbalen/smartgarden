create table sensor_data (
  id                  bigserial,
  sector              integer,
  soil_moisture       boolean      not null,
  luminosity          integer      not null check (luminosity between 0 and 1023),
  low_light           boolean      not null,
  reservoir_volume_l  numeric(6,2) not null check (reservoir_volume_l >= 0),
  low_reservoir       boolean      not null,
  recorded_at         timestamptz  not null,
  created_at          timestamptz  not null default now(),

  primary key (id, sector)
);

alter table sensor_data enable row level security;

create policy "Allow anon insert"
  on sensor_data for insert to anon
  with check (true);

create policy "Allow anon select"
  on sensor_data for select to anon
  using (true);

create table device_config (
  sector                   integer      primary key,
  sunrise_time             time        not null,
  sunset_time              time        not null,
  iteration_interval_s     integer      not null check (iteration_interval_s > 0),
  ldr_threshold            integer      not null check (ldr_threshold between 0 and 1023),
  min_reservoir_volume_l   numeric(6,2) not null check (min_reservoir_volume_l >= 0),
  tank_radius_cm           numeric(6,2) not null check (tank_radius_cm > 0),
  tank_height_cm           numeric(6,2) not null check (tank_height_cm > 0),
  updated_at               timestamptz  not null default now()
);

alter table device_config enable row level security;

create policy "Allow anon select"
  on device_config for select to anon
  using (true);

insert into device_config (sector, sunrise_time, sunset_time, iteration_interval_s, ldr_threshold, min_reservoir_volume_l, tank_radius_cm, tank_height_cm)
values (1, '06:00', '18:00', 15, 40, 5.0, 15.0, 40.0);
