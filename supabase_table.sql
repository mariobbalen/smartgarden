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
